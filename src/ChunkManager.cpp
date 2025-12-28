#include "ChunkManager.h"
#include <iostream>
#include <algorithm>

// =============================
// Utility
// =============================
long long ChunkManager::makeKey(int x, int z) const {
    return (static_cast<long long>(x) << 32) ^ (static_cast<unsigned int>(z));
}

// =============================
// Constructor / Destructor
// =============================
ChunkManager::ChunkManager(int rd, const std::string& worldName)
    : renderDistance(rd),
    renderDistanceSquared(rd* rd),
    lastPlayerChunkX(INT_MAX),
    lastPlayerChunkZ(INT_MAX),
    shouldStop(false),
    worldSave(std::make_unique<WorldSave>(worldName))
{
    generationThread = std::thread(&ChunkManager::generationWorker, this);
}

ChunkManager::~ChunkManager() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        shouldStop = true;
    }
    queueCV.notify_all();
    generationThread.join();

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
        std::pair<int, int> coords;

        {
            std::unique_lock<std::mutex> lock(mutex);
            queueCV.wait(lock, [&] {
                return shouldStop || !generationQueue.empty();
                });

            if (shouldStop) return;

            coords = generationQueue.front();
            generationQueue.pop();
        }

        Chunk* chunk = new Chunk(coords.first, coords.second);
        TerrainGenerator::generateFlatTerrain(*chunk);

        {
            std::lock_guard<std::mutex> lock(mutex);
            readyChunks.push(chunk);
            chunksBeingGenerated.erase({ coords.first, coords.second });
            queuedChunks.erase(makeKey(coords.first, coords.second));
        }
    }
}

// =============================
// Main Update - GPU OPTIMIZED VERSION
// =============================
void ChunkManager::update(float playerX, float playerZ) {
    auto [playerChunkX, playerChunkZ] = worldToChunkCoords(playerX, playerZ);

    if (playerChunkX != lastPlayerChunkX || playerChunkZ != lastPlayerChunkZ) {
        lastPlayerChunkX = playerChunkX;
        lastPlayerChunkZ = playerChunkZ;

        updateDesiredChunks(playerChunkX, playerChunkZ);
    }

    processReadyChunks();

    // Auto-save check
    if (worldSave) {
        worldSave->autoSaveCheck();
    }
}

// =============================
// Update desired chunks in radius
// =============================
void ChunkManager::updateDesiredChunks(int pcx, int pcz) {
    std::vector<ChunkDistanceEntry> ordered;
    ordered.reserve((renderDistance * 2 + 1) * (renderDistance * 2 + 1));

    for (int dx = -renderDistance; dx <= renderDistance; dx++) {
        for (int dz = -renderDistance; dz <= renderDistance; dz++) {
            int distSq = dx * dx + dz * dz;
            if (distSq <= renderDistanceSquared) {
                ordered.push_back({ pcx + dx, pcz + dz, distSq });
            }
        }
    }

    std::sort(ordered.begin(), ordered.end(), [](const ChunkDistanceEntry& a, const ChunkDistanceEntry& b) {
        return a.distSq < b.distSq;
        });

    std::unordered_set<long long> desired;
    desired.reserve(ordered.size());
    for (const auto& entry : ordered) {
        desired.insert(makeKey(entry.x, entry.z));
    }

    unloadDistantChunks(pcx, pcz);

    {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& entry : ordered) {
            long long key = makeKey(entry.x, entry.z);
            if (chunks.count(key) || queuedChunks.count(key)) continue;

            queuedChunks.insert(key);
            generationQueue.push({ entry.x, entry.z });
            chunksBeingGenerated.insert({ entry.x, entry.z });
        }
    }

    queueCV.notify_all();
}

// =============================
// Unload chunks outside radius + buffer
// =============================
void ChunkManager::unloadDistantChunks(int playerChunkX, int playerChunkZ) {
    std::vector<std::pair<int, int>> toUnload;

    {
        std::lock_guard<std::mutex> lock(chunksMutex);
        for (auto& [key, chunk] : chunks) {
            int cx = static_cast<int>(key >> 32);
            int cz = static_cast<int>(key & 0xffffffff);
            int dx = cx - playerChunkX;
            int dz = cz - playerChunkZ;
            float distance = std::sqrt(dx * dx + dz * dz);

            if (distance > renderDistance + 2) {
                toUnload.push_back({ cx, cz });
            }
        }
    }

    for (auto& [cx, cz] : toUnload) {
        unloadChunk(cx, cz);
    }

    if (!toUnload.empty()) {
        std::cout << "Unloaded " << toUnload.size() << " chunks\n";
    }
}

// =============================
// Chunk Load / Unload
// =============================
bool ChunkManager::isChunkLoaded(int cx, int cz) {
    std::lock_guard<std::mutex> lock(chunksMutex);
    return chunks.count(makeKey(cx, cz)) > 0;
}

void ChunkManager::loadChunk(int cx, int cz) {
    std::lock_guard<std::mutex> lock(mutex);
    long long key = makeKey(cx, cz);
    if (queuedChunks.count(key)) return;

    queuedChunks.insert(key);
    generationQueue.push({ cx, cz });
    queueCV.notify_all();
}

void ChunkManager::unloadChunk(int cx, int cz) {
    long long key = makeKey(cx, cz);

    std::lock_guard<std::mutex> lock(chunksMutex);
    auto it = chunks.find(key);
    if (it != chunks.end()) {
        delete it->second;
        chunks.erase(it);
    }
}

// =============================
// UPDATED: Integrate Ready Chunks with Flood Fill
// =============================
void ChunkManager::processReadyChunks() {
    const int maxPerFrame = 4;  // Reduced to prevent FPS drops

    for (int i = 0; i < maxPerFrame; i++) {
        Chunk* chunk = nullptr;

        {
            std::lock_guard<std::mutex> lock(mutex);
            if (readyChunks.empty()) return;
            chunk = readyChunks.front();
            readyChunks.pop();
            long long key = makeKey(chunk->chunkX, chunk->chunkZ);
            chunks[key] = chunk;
        }

        // Apply saved modifications
        std::vector<ModifiedBlock> modifications;
        worldSave->loadChunkModifications(chunk->chunkX, chunk->chunkZ, modifications);
        for (auto& mod : modifications) {
            int localX = mod.x - chunk->chunkX * CHUNK_SIZE_X;
            int localZ = mod.z - chunk->chunkZ * CHUNK_SIZE_Z;
            chunk->setBlock(localX, mod.y, localZ, mod.type);
        }

        // Calculate sky light (includes internal propagation)
        chunk->calculateSkyLight(15);  // ALWAYS 15

        // Link neighbors
        linkChunkNeighbors(chunk);

        // Cross-chunk propagation
        chunk->propagateSkyLightFloodFill();

        // Build mesh
        chunk->buildMesh();
    }
}


// =============================
// Neighbor Linking
// =============================
void ChunkManager::linkChunkNeighbors(Chunk* chunk) {
    static const int dx[4] = { 0,0,1,-1 };
    static const int dz[4] = { 1,-1,0,0 };

    std::lock_guard<std::mutex> lock(chunksMutex);
    for (int i = 0; i < 4; i++) {
        long long key = makeKey(chunk->chunkX + dx[i], chunk->chunkZ + dz[i]);
        auto it = chunks.find(key);
        if (it != chunks.end()) {
            chunk->setNeighbor(i, it->second);
            it->second->setNeighbor(i ^ 1, chunk);
            it->second->buildMesh();
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
Block* ChunkManager::getBlockAt(int worldX, int worldY, int worldZ) {
    int cx = worldX / CHUNK_SIZE_X;
    if (worldX < 0 && worldX % CHUNK_SIZE_X != 0) cx--;
    int cz = worldZ / CHUNK_SIZE_Z;
    if (worldZ < 0 && worldZ % CHUNK_SIZE_Z != 0) cz--;

    int lx = worldX - cx * CHUNK_SIZE_X;
    int lz = worldZ - cz * CHUNK_SIZE_Z;

    std::lock_guard<std::mutex> lock(chunksMutex);
    auto it = chunks.find(makeKey(cx, cz));
    if (it == chunks.end()) return nullptr;

    return new Block(it->second->getBlock(lx, worldY, lz));
}

// =============================
// World -> Chunk coords
// =============================
std::pair<int, int> ChunkManager::worldToChunkCoords(float x, float z) {
    int cx = static_cast<int>(std::floor(x / CHUNK_SIZE_X));
    int cz = static_cast<int>(std::floor(-z / CHUNK_SIZE_Z));
    return { cx, cz };
}

bool ChunkManager::setBlockAt(int worldX, int worldY, int worldZ, BlockType type) {
    int chunkX = worldX / CHUNK_SIZE_X;
    if (worldX < 0 && worldX % CHUNK_SIZE_X != 0) chunkX--;

    int chunkZ = worldZ / CHUNK_SIZE_Z;
    if (worldZ < 0 && worldZ % CHUNK_SIZE_Z != 0) chunkZ--;

    int localX = worldX - chunkX * CHUNK_SIZE_X;
    int localZ = worldZ - chunkZ * CHUNK_SIZE_Z;

    std::lock_guard<std::mutex> lock(chunksMutex);
    long long key = makeKey(chunkX, chunkZ);
    auto it = chunks.find(key);
    if (it == chunks.end()) return false;

    it->second->setBlock(localX, worldY, localZ, type);

    // SAVE THE MODIFICATION
    worldSave->saveBlockChange(worldX, worldY, worldZ, type);

    return true;
}

// =============================
// UPDATED: Rebuild with Comprehensive Flood Fill
// =============================
// CRITICAL: When block changes, do proper increase/decrease passes
void ChunkManager::rebuildChunkMeshAt(int worldX, int worldY, int worldZ) {
    int chunkX = worldX / CHUNK_SIZE_X;
    if (worldX < 0 && worldX % CHUNK_SIZE_X != 0) chunkX--;

    int chunkZ = worldZ / CHUNK_SIZE_Z;
    if (worldZ < 0 && worldZ % CHUNK_SIZE_Z != 0) chunkZ--;

    std::lock_guard<std::mutex> lock(chunksMutex);

    // Recalculate light for center + neighbors
    long long key = makeKey(chunkX, chunkZ);
    auto it = chunks.find(key);
    if (it != chunks.end()) {
        it->second->calculateSkyLight(15);
    }

    static const int dx[4] = { 0, 0, 1, -1 };
    static const int dz[4] = { 1, -1, 0, 0 };

    for (int dir = 0; dir < 4; dir++) {
        long long neighborKey = makeKey(chunkX + dx[dir], chunkZ + dz[dir]);
        auto neighbor = chunks.find(neighborKey);
        if (neighbor != chunks.end()) {
            neighbor->second->calculateSkyLight(15);
        }
    }

    // Cross-chunk propagation from center + neighbors
    if (it != chunks.end()) {
        it->second->propagateSkyLightFloodFill();
    }

    for (int dir = 0; dir < 4; dir++) {
        long long neighborKey = makeKey(chunkX + dx[dir], chunkZ + dz[dir]);
        auto neighbor = chunks.find(neighborKey);
        if (neighbor != chunks.end()) {
            neighbor->second->propagateSkyLightFloodFill();
        }
    }

    // Rebuild meshes
    if (it != chunks.end()) {
        it->second->buildMesh();
    }

    for (int dir = 0; dir < 4; dir++) {
        long long neighborKey = makeKey(chunkX + dx[dir], chunkZ + dz[dir]);
        auto neighbor = chunks.find(neighborKey);
        if (neighbor != chunks.end()) {
            neighbor->second->buildMesh();
        }
    }
}

std::vector<Chunk*> ChunkManager::getLoadedChunks() {
    std::vector<Chunk*> loaded;
    std::lock_guard<std::mutex> lock(chunksMutex);
    loaded.reserve(chunks.size());
    for (auto& pair : chunks) {
        loaded.push_back(pair.second);
    }
    return loaded;
}