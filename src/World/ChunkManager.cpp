#include "World/ChunkManager.h"
#include <iostream>
#include <algorithm>
#include <climits>
#include <cmath>

// =============================
// 3D KEY GENERATION
// =============================
long long ChunkManager::makeKey(int x, int y, int z) const {
    return (static_cast<long long>(x + 100000) << 42) |
        (static_cast<long long>(y + 10000) << 21) |
        static_cast<long long>(z + 100000);
}

// =============================
// Constructor / Destructor
// =============================
ChunkManager::ChunkManager(int rd, const std::string& worldName)
    : renderDistance(rd),
    renderDistanceSquared(rd* rd),
    verticalRenderDistance(4),  // CONFIGURABLE: Default 4 chunks up/down (64 blocks)
    lastPlayerChunkX(INT_MAX),
    lastPlayerChunkY(INT_MAX),
    lastPlayerChunkZ(INT_MAX),
    shouldStop(false),
    worldSave(std::make_unique<WorldSave>(worldName)) {

    // MAXIMIZE CPU: Use all cores. High-speed flight requires massive throughput.
    unsigned int threadCount = std::max(2u, std::thread::hardware_concurrency());
    for (unsigned int i = 0; i < threadCount; ++i) {
        workerThreads.push_back(std::thread(&ChunkManager::generationWorker, this));
    }
}

ChunkManager::~ChunkManager() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        shouldStop = true;
    }
    queueCV.notify_all();

    for (auto& t : workerThreads) {
        if (t.joinable()) t.join();
    }

    for (auto& [_, chunk] : chunks) {
        delete chunk;
    }
    chunks.clear();
}

// =============================
// Threaded Generation
// =============================
void ChunkManager::generationWorker() {
    while (true) {
        std::tuple<int, int, int> coords;
        {
            std::unique_lock<std::mutex> lock(mutex);
            queueCV.wait(lock, [&] { return shouldStop || !generationQueue.empty(); });
            if (shouldStop) return;

            coords = generationQueue.front();
            generationQueue.pop();
            chunksBeingGenerated.insert(coords);
        }

        auto [cx, cy, cz] = coords;
        Chunk* chunk = new Chunk(cx, cy, cz);
        TerrainGenerator::generateTerrain(*chunk);

        // NEW: Load modifications BEFORE meshing
        std::vector<ModifiedBlock> modifications;
        worldSave->loadChunkModifications(cx, cz, modifications);
        for (auto& mod : modifications) {
            int localY = mod.y - cy * CHUNK_SIZE_Y;
            if (localY >= 0 && localY < CHUNK_SIZE_Y) {
                chunk->setBlock(mod.x - cx * CHUNK_SIZE_X, localY, mod.z - cz * CHUNK_SIZE_Z, mod.type);
            }
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            readyChunks.push(chunk);
            chunksBeingGenerated.erase(coords);
        }
    }
}

// =============================
// Main Update
// =============================
void ChunkManager::update(float playerX, float playerY, float playerZ) {
    auto [playerChunkX, playerChunkY, playerChunkZ] = worldToChunkCoords(playerX, playerY, playerZ);

    // If moved, RE-PRIORITIZE everything immediately
    if (playerChunkX != lastPlayerChunkX ||
        playerChunkY != lastPlayerChunkY ||
        playerChunkZ != lastPlayerChunkZ) {

        lastPlayerChunkX = playerChunkX;
        lastPlayerChunkY = playerChunkY;
        lastPlayerChunkZ = playerChunkZ;

        updateDesiredChunks(playerChunkX, playerChunkY, playerChunkZ);
    }

    processReadyChunks();

    if (worldSave) worldSave->autoSaveCheck();
}

// =============================
// Update desired chunks
// =============================
void ChunkManager::updateDesiredChunks(int pcx, int pcy, int pcz) {
    // 1. CLEAR QUEUE (Maintain Jet Speed)
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::queue<std::tuple<int, int, int>> empty;
        std::swap(generationQueue, empty);
        queuedChunks.clear();
        for (const auto& c : chunksBeingGenerated) {
            queuedChunks.insert(makeKey(std::get<0>(c), std::get<1>(c), std::get<2>(c)));
        }
    }

    std::vector<ChunkDistanceEntry> ordered;

    // DISK SHAPE: Circular XZ, Linear Y
    // XZ Plane: renderDistance (e.g., 6 = 96 block radius)
    // Y Axis: verticalRenderDistance (e.g., 4 = 4 chunks up + 4 down = 64 blocks total range)

    for (int dx = -renderDistance; dx <= renderDistance; dx++) {
        for (int dz = -renderDistance; dz <= renderDistance; dz++) {

            // CIRCULAR check for the XZ plane
            int distSqXZ = dx * dx + dz * dz;
            if (distSqXZ > renderDistanceSquared) continue;

            // LINEAR check for Y axis (creates disk shape)
            for (int dy = -verticalRenderDistance; dy <= verticalRenderDistance; dy++) {
                int cx = pcx + dx;
                int cy = pcy + dy;
                int cz = pcz + dz;

                // World height bounds check
                if (cy < 0 || cy >= 6250) continue;

                // We use distSqXZ for sorting so it still prioritizes 
                // chunks closest to you horizontally
                ordered.push_back({ cx, cy, cz, distSqXZ });
            }
        }
    }

    // Sort by horizontal distance
    std::sort(ordered.begin(), ordered.end(), [](const ChunkDistanceEntry& a, const ChunkDistanceEntry& b) {
        return a.distSq < b.distSq;
        });

    unloadDistantChunks(pcx, pcy, pcz);

    {
        std::lock_guard<std::mutex> lock(mutex);
        std::lock_guard<std::mutex> chunkLock(chunksMutex);
        for (const auto& entry : ordered) {
            long long key = makeKey(entry.x, entry.y, entry.z);
            if (chunks.count(key) == 0 && queuedChunks.count(key) == 0) {
                queuedChunks.insert(key);
                generationQueue.push({ entry.x, entry.y, entry.z });
            }
        }
    }
    queueCV.notify_all();
}

// =============================
// Integrate Ready Chunks - FIXED RACE CONDITION
// =============================
void ChunkManager::processReadyChunks() {
    const int maxPerFrame = 24;

    for (int i = 0; i < maxPerFrame; i++) {
        Chunk* chunk = nullptr;
        long long key;

        {
            std::lock_guard<std::mutex> lock(mutex);
            if (readyChunks.empty()) return;

            chunk = readyChunks.front();
            readyChunks.pop();

            key = makeKey(chunk->chunkX, chunk->chunkY, chunk->chunkZ);
        }

        // 1. Add to map FIRST
        {
            std::lock_guard<std::mutex> cLock(chunksMutex);
            if (chunks.count(key)) {
                delete chunk;
                continue;
            }
            chunks[key] = chunk;
        }

        // 2. Link ALL neighbors (bi-directional)
        linkChunkNeighbors(chunk);

        // 3. NOW build meshes (all links are complete)
        chunk->buildMesh();

        // 4. Rebuild neighbors ONLY if they're already linked to us
        {
            static const int dx[6] = { 0, 0, 1, -1, 0, 0 };
            static const int dy[6] = { 0, 0, 0, 0, 1, -1 };
            static const int dz[6] = { 1, -1, 0, 0, 0, 0 };

            std::lock_guard<std::mutex> cLock(chunksMutex);
            for (int dir = 0; dir < 6; dir++) {
                // Check if neighbor exists AND is already linked back to us
                Chunk* neighbor = chunk->getNeighbor(dir);
                if (neighbor) {
                    // Verify bi-directional link is complete
                    if (neighbor->getNeighbor(dir ^ 1) == chunk) {
                        neighbor->buildMesh();
                    }
                }
            }
        }

        // 5. Clean up
        {
            std::lock_guard<std::mutex> lock(mutex);
            queuedChunks.erase(key);
        }
    }
}

// =============================
// Unload distant chunks
// =============================
void ChunkManager::unloadDistantChunks(int pcx, int pcy, int pcz) {
    std::vector<long long> toUnload;
    // Unload buffer is larger than load distance to prevent "flickering" at the edge
    const int limit = (renderDistance + 2) * (renderDistance + 2);

    {
        std::lock_guard<std::mutex> lock(chunksMutex);
        for (auto& [key, chunk] : chunks) {
            int dx = chunk->chunkX - pcx;
            int dy = chunk->chunkY - pcy;
            int dz = chunk->chunkZ - pcz;
            if (dx * dx + dy * dy + dz * dz > limit) {
                toUnload.push_back(key);
            }
        }

        for (long long key : toUnload) {
            delete chunks[key];
            chunks.erase(key);
        }
    }
}


// =============================
// Chunk load/unload
// =============================
bool ChunkManager::isChunkLoaded(int cx, int cy, int cz) {
    std::lock_guard<std::mutex> lock(chunksMutex);
    return chunks.count(makeKey(cx, cy, cz)) > 0;
}

void ChunkManager::loadChunk(int cx, int cy, int cz) {
    std::lock_guard<std::mutex> lock(mutex);
    long long key = makeKey(cx, cy, cz);
    if (queuedChunks.count(key)) return;

    queuedChunks.insert(key);
    generationQueue.push({ cx, cy, cz });
    queueCV.notify_all();
}

void ChunkManager::unloadChunk(int cx, int cy, int cz) {
    long long key = makeKey(cx, cy, cz);

    std::lock_guard<std::mutex> lock(chunksMutex);
    auto it = chunks.find(key);
    if (it != chunks.end()) {
        delete it->second;
        chunks.erase(it);
    }
}

// =============================
// Neighbor Linking
// =============================
void ChunkManager::linkChunkNeighbors(Chunk* chunk) {
    static const int dx[6] = { 0, 0, 1, -1, 0, 0 };
    static const int dy[6] = { 0, 0, 0, 0, 1, -1 };
    static const int dz[6] = { 1, -1, 0, 0, 0, 0 };

    std::lock_guard<std::mutex> lock(chunksMutex);
    for (int i = 0; i < 6; i++) {
        long long key = makeKey(chunk->chunkX + dx[i],
            chunk->chunkY + dy[i],
            chunk->chunkZ + dz[i]);

        auto it = chunks.find(key);
        if (it != chunks.end()) {
            // Bi-directional pointers only. Extremely fast.
            chunk->setNeighbor(i, it->second);
            it->second->setNeighbor(i ^ 1, chunk);
        }
    }
}

// =============================
// Rendering
// =============================
void ChunkManager::render() {
    std::lock_guard<std::mutex> lock(chunksMutex);
    for (auto& [_, chunk] : chunks) {
        chunk->render();
    }
}

void ChunkManager::renderType(BlockType type) {
    std::lock_guard<std::mutex> lock(chunksMutex);
    for (auto& [_, chunk] : chunks) {
        chunk->renderType(type);
    }
}

// =============================
// Block Query
// =============================

// COMPATIBILITY: Restored to return Block* for external files
Block* ChunkManager::getBlockAt(int worldX, int worldY, int worldZ) {
    // We get the block data (optimized), then wrap it in a pointer
    // This allows external files to 'delete' it if they are designed that way
    Block data = getBlockData(worldX, worldY, worldZ);
    return new Block(data.type);
}

// OPTIMIZED: Returns Block value (no heap allocation)
Block ChunkManager::getBlockData(int worldX, int worldY, int worldZ) {
    int cx = worldX / CHUNK_SIZE_X; if (worldX < 0 && worldX % CHUNK_SIZE_X != 0) cx--;
    int cy = worldY / CHUNK_SIZE_Y; if (worldY < 0 && worldY % CHUNK_SIZE_Y != 0) cy--;
    int cz = worldZ / CHUNK_SIZE_Z; if (worldZ < 0 && worldZ % CHUNK_SIZE_Z != 0) cz--;

    int lx = worldX - cx * CHUNK_SIZE_X;
    int ly = worldY - cy * CHUNK_SIZE_Y;
    int lz = worldZ - cz * CHUNK_SIZE_Z;

    std::lock_guard<std::mutex> lock(chunksMutex);
    auto it = chunks.find(makeKey(cx, cy, cz));

    if (it == chunks.end()) return Block(BlockType::AIR);

    // Directly return the value from the chunk
    return it->second->getBlock(lx, ly, lz);
}

// =============================
// World -> Chunk coords
// =============================
std::tuple<int, int, int> ChunkManager::worldToChunkCoords(float x, float y, float z) {
    int cx = static_cast<int>(std::floor(x / CHUNK_SIZE_X));
    int cy = static_cast<int>(std::floor(y / CHUNK_SIZE_Y));
    int cz = static_cast<int>(std::floor(-z / CHUNK_SIZE_Z));
    return { cx, cy, cz };
}

// =============================
// Set Block
// =============================
bool ChunkManager::setBlockAt(int worldX, int worldY, int worldZ, BlockType type) {
    int chunkX = worldX / CHUNK_SIZE_X; if (worldX < 0 && worldX % CHUNK_SIZE_X != 0) chunkX--;
    int chunkY = worldY / CHUNK_SIZE_Y; if (worldY < 0 && worldY % CHUNK_SIZE_Y != 0) chunkY--;
    int chunkZ = worldZ / CHUNK_SIZE_Z; if (worldZ < 0 && worldZ % CHUNK_SIZE_Z != 0) chunkZ--;

    int localX = worldX - chunkX * CHUNK_SIZE_X;
    int localY = worldY - chunkY * CHUNK_SIZE_Y;
    int localZ = worldZ - chunkZ * CHUNK_SIZE_Z;

    std::lock_guard<std::mutex> lock(chunksMutex);
    long long key = makeKey(chunkX, chunkY, chunkZ);
    auto it = chunks.find(key);
    if (it == chunks.end()) return false;

    it->second->setBlock(localX, localY, localZ, type);
    worldSave->saveBlockChange(worldX, worldY, worldZ, type);

    return true;
}

// =============================
// Rebuild Mesh
// =============================
void ChunkManager::rebuildChunkMeshAt(int worldX, int worldY, int worldZ) {
    int chunkX = worldX / CHUNK_SIZE_X; if (worldX < 0 && worldX % CHUNK_SIZE_X != 0) chunkX--;
    int chunkY = worldY / CHUNK_SIZE_Y; if (worldY < 0 && worldY % CHUNK_SIZE_Y != 0) chunkY--;
    int chunkZ = worldZ / CHUNK_SIZE_Z; if (worldZ < 0 && worldZ % CHUNK_SIZE_Z != 0) chunkZ--;

    std::lock_guard<std::mutex> lock(chunksMutex);

    long long key = makeKey(chunkX, chunkY, chunkZ);
    auto it = chunks.find(key);
    if (it != chunks.end()) it->second->buildMesh();

    static const int dx[6] = { 0,0,1,-1,0,0 };
    static const int dy[6] = { 0,0,0,0,1,-1 };
    static const int dz[6] = { 1,-1,0,0,0,0 };

    for (int dir = 0; dir < 6; dir++) {
        long long neighborKey = makeKey(chunkX + dx[dir], chunkY + dy[dir], chunkZ + dz[dir]);
        auto neighbor = chunks.find(neighborKey);
        if (neighbor != chunks.end()) neighbor->second->buildMesh();
    }
}

// =============================
// Get Loaded Chunks
// =============================
std::vector<Chunk*> ChunkManager::getLoadedChunks() {
    std::vector<Chunk*> loaded;
    std::lock_guard<std::mutex> lock(chunksMutex);
    loaded.reserve(chunks.size());
    for (auto& pair : chunks) loaded.push_back(pair.second);
    return loaded;
}

// =============================
// Set Vertical Render Distance
// =============================
void ChunkManager::setVerticalRenderDistance(int distance) {
    verticalRenderDistance = distance;
    std::cout << "Vertical render distance set to: " << distance << " chunks (" << (distance * 16) << " blocks)\n";
}

int ChunkManager::getVerticalRenderDistance() const {
    return verticalRenderDistance;
}