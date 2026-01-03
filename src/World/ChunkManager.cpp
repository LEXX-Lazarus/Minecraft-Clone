#include "World/ChunkManager.h"
#include "World/TerrainGenerator.h"
#include <iostream>
#include <algorithm>
#include <climits>
#include <cmath>

long long ChunkManager::makeKey(int x, int y, int z) const {
    return (static_cast<long long>(x + 100000) << 42) |
        (static_cast<long long>(y + 10000) << 21) |
        static_cast<long long>(z + 100000);
}

ChunkManager::ChunkManager(int rd, const std::string& worldName)
    : renderDistance(rd),
    renderDistanceSquared(rd* rd),
    verticalRenderDistance(4),
    lastPlayerChunkX(INT_MAX),
    lastPlayerChunkY(INT_MAX),
    lastPlayerChunkZ(INT_MAX),
    shouldStop(false),
    textureAtlas(nullptr),
    worldSave(std::make_unique<WorldSave>(worldName)) {

    // Use more threads for generation
    unsigned int threadCount = std::min(8u, std::max(4u, std::thread::hardware_concurrency()));
    workerThreads.reserve(threadCount);
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

void ChunkManager::setTextureAtlas(TextureAtlas* atlas) {
    textureAtlas = atlas;
}

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

        // Load modifications if they exist
        std::vector<ModifiedBlock> modifications;
        worldSave->loadChunkModifications(cx, cz, modifications);

        if (!modifications.empty()) {
            int chunkWorldYMin = cy * CHUNK_SIZE_Y;
            int chunkWorldYMax = chunkWorldYMin + CHUNK_SIZE_Y - 1;

            for (const auto& mod : modifications) {
                if (mod.y < chunkWorldYMin || mod.y > chunkWorldYMax) continue;

                int localX = mod.x - cx * CHUNK_SIZE_X;
                int localY = mod.y - cy * CHUNK_SIZE_Y;
                int localZ = mod.z - cz * CHUNK_SIZE_Z;

                if (localX >= 0 && localX < CHUNK_SIZE_X &&
                    localY >= 0 && localY < CHUNK_SIZE_Y &&
                    localZ >= 0 && localZ < CHUNK_SIZE_Z) {
                    chunk->setBlock(localX, localY, localZ, mod.type);
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            readyChunks.push(chunk);
            chunksBeingGenerated.erase(coords);
        }
    }
}

void ChunkManager::update(float playerX, float playerY, float playerZ) {
    auto [playerChunkX, playerChunkY, playerChunkZ] = worldToChunkCoords(playerX, playerY, playerZ);

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

void ChunkManager::updateDesiredChunks(int pcx, int pcy, int pcz) {
    // 1. Filter the existing queue to remove chunks that have moved too far away
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::queue<std::tuple<int, int, int>> filteredQueue;
        const int keepDistance = (renderDistance + 6) * (renderDistance + 6);

        while (!generationQueue.empty()) {
            auto coords = generationQueue.front();
            generationQueue.pop();

            int dx = std::get<0>(coords) - pcx;
            int dy = std::get<1>(coords) - pcy;
            int dz = std::get<2>(coords) - pcz;

            if (dx * dx + dy * dy + dz * dz <= keepDistance) {
                filteredQueue.push(coords);
            }
            else {
                queuedChunks.erase(makeKey(std::get<0>(coords), std::get<1>(coords), std::get<2>(coords)));
            }
        }
        generationQueue = std::move(filteredQueue);
    }

    // 2. Identify all chunks within the render distance
    std::vector<ChunkDistanceEntry> ordered;
    int estimatedSize = (renderDistance * 2) * (renderDistance * 2) * (verticalRenderDistance * 2);
    ordered.reserve(estimatedSize);

    for (int dx = -renderDistance; dx <= renderDistance; dx++) {
        for (int dz = -renderDistance; dz <= renderDistance; dz++) {
            int distSqXZ = dx * dx + dz * dz;
            if (distSqXZ > renderDistanceSquared) continue;

            for (int dy = -verticalRenderDistance; dy <= verticalRenderDistance; dy++) {
                int cx = pcx + dx;
                int cy = pcy + dy;
                int cz = pcz + dz;

                if (cy < 0 || cy >= 6250) continue; // Boundary check
                ordered.push_back({ cx, cy, cz, distSqXZ });
            }
        }
    }

    // 3. Sort by distance and movement direction (Priority Loading)
    int moveX = pcx - lastPlayerChunkX;
    int moveZ = pcz - lastPlayerChunkZ;

    std::sort(ordered.begin(), ordered.end(), [pcx, pcz, moveX, moveZ](const ChunkDistanceEntry& a, const ChunkDistanceEntry& b) {
        if (a.distSq != b.distSq) return a.distSq < b.distSq;

        if (moveX != 0 || moveZ != 0) {
            int aDot = (a.x - pcx) * moveX + (a.z - pcz) * moveZ;
            int bDot = (b.x - pcx) * moveX + (b.z - pcz) * moveZ;
            if (aDot != bDot) return aDot > bDot;
        }
        return false;
        });

    // 4. Fill the generation queue
    // We removed the 'break' to ensure we check the whole radius.
    // We only limit the total queue size to prevent massive memory spikes.
    int newlyQueued = 0;
    const int maxTotalQueue = 128; // Allow a large enough buffer for the background threads

    for (const auto& entry : ordered) {
        long long key = makeKey(entry.x, entry.y, entry.z);

        // Check if already in memory
        {
            std::lock_guard<std::mutex> chunkLock(chunksMutex);
            if (chunks.find(key) != chunks.end()) continue;
        }

        // Check if already in queue
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (queuedChunks.find(key) != queuedChunks.end()) continue;

            if (generationQueue.size() >= maxTotalQueue) break;

            queuedChunks.insert(key);
            generationQueue.push({ entry.x, entry.y, entry.z });
            newlyQueued++;
        }
    }

    if (newlyQueued > 0) {
        queueCV.notify_all();
    }

    // 5. Clean up distant chunks
    unloadDistantChunks(pcx, pcy, pcz);
}

void ChunkManager::processReadyChunks() {
    int readyCount = 0;
    {
        std::lock_guard<std::mutex> lock(mutex);
        readyCount = readyChunks.size();
    }

    // AGGRESSIVE LOADING: Process chunks faster to keep up with generation
    // When stationary: load chunks as fast as GPU can handle (like Minecraft)
    // When moving fast: moderate rate to maintain FPS
    int maxPerFrame = 32;  // Base rate - faster than before
    if (readyCount > 50) maxPerFrame = 48;   // Moderate backlog - speed up
    if (readyCount > 100) maxPerFrame = 64;  // Heavy backlog - go faster
    if (readyCount > 200) maxPerFrame = 96;  // Emergency - clear backlog quickly

    std::vector<Chunk*> chunksToProcess;
    std::vector<long long> keys;
    chunksToProcess.reserve(maxPerFrame);
    keys.reserve(maxPerFrame);

    {
        std::lock_guard<std::mutex> lock(mutex);
        for (int i = 0; i < maxPerFrame && !readyChunks.empty(); i++) {
            Chunk* chunk = readyChunks.front();
            readyChunks.pop();
            chunksToProcess.push_back(chunk);
            keys.push_back(makeKey(chunk->chunkX, chunk->chunkY, chunk->chunkZ));
        }
    }

    // Track which neighbors need rebuilds
    std::unordered_set<long long> chunksNeedingRebuild;

    {
        std::lock_guard<std::mutex> cLock(chunksMutex);

        for (size_t i = 0; i < chunksToProcess.size(); i++) {
            Chunk* chunk = chunksToProcess[i];
            long long key = keys[i];

            if (chunks.count(key)) {
                delete chunk;
                continue;
            }

            chunks[key] = chunk;
            linkChunkNeighborsUnsafe(chunk);
            
            // Build initial mesh
            chunk->buildMesh(textureAtlas);

            // Mark neighbors for rebuild
            static const int dx[6] = { 0, 0, 1, -1, 0, 0 };
            static const int dy[6] = { 0, 0, 0, 0, 1, -1 };
            static const int dz[6] = { 1, -1, 0, 0, 0, 0 };

            for (int dir = 0; dir < 6; dir++) {
                long long neighborKey = makeKey(
                    chunk->chunkX + dx[dir],
                    chunk->chunkY + dy[dir],
                    chunk->chunkZ + dz[dir]
                );
                
                if (chunks.count(neighborKey)) {
                    chunksNeedingRebuild.insert(neighborKey);
                }
            }
        }

        // Adaptive neighbor rebuilding - more generous limits
        int maxNeighborRebuilds = (readyCount < 50) ? 48 : 24;
        int rebuildsThisFrame = 0;
        
        for (long long neighborKey : chunksNeedingRebuild) {
            if (rebuildsThisFrame >= maxNeighborRebuilds) break;
            
            auto it = chunks.find(neighborKey);
            if (it != chunks.end()) {
                it->second->buildMesh(textureAtlas);
                rebuildsThisFrame++;
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex);
        for (long long key : keys) {
            queuedChunks.erase(key);
        }
    }
}

void ChunkManager::linkChunkNeighborsUnsafe(Chunk* chunk) {
    static const int dx[6] = { 0, 0, 1, -1, 0, 0 };
    static const int dy[6] = { 0, 0, 0, 0, 1, -1 };
    static const int dz[6] = { 1, -1, 0, 0, 0, 0 };

    for (int i = 0; i < 6; i++) {
        long long key = makeKey(chunk->chunkX + dx[i],
            chunk->chunkY + dy[i],
            chunk->chunkZ + dz[i]);

        auto it = chunks.find(key);
        if (it != chunks.end()) {
            chunk->setNeighbor(i, it->second);
            it->second->setNeighbor(i ^ 1, chunk);
        }
    }
}

void ChunkManager::unloadDistantChunks(int pcx, int pcy, int pcz) {
    std::vector<long long> toUnload;
    
    // ========== FIX #5: INCREASED UNLOAD BUFFER ==========
    // OLD CODE: renderDistance + 2 (too aggressive, caused premature unloading)
    // NEW CODE: renderDistance + 6 (gives breathing room when moving fast)
    const int unloadBuffer = 6;
    const int limit = (renderDistance + unloadBuffer) * (renderDistance + unloadBuffer);

    {
        std::lock_guard<std::mutex> lock(chunksMutex);
        toUnload.reserve(chunks.size() / 4);

        for (auto& [key, chunk] : chunks) {
            int dx = chunk->chunkX - pcx;
            int dy = chunk->chunkY - pcy;
            int dz = chunk->chunkZ - pcz;

            if (dx * dx + dy * dy + dz * dz > limit) {
                toUnload.push_back(key);
            }
        }

        // Unlink neighbors
        for (long long key : toUnload) {
            Chunk* chunk = chunks[key];
            for (int dir = 0; dir < 6; dir++) {
                Chunk* neighbor = chunk->getNeighbor(dir);
                if (neighbor) {
                    neighbor->setNeighbor(dir ^ 1, nullptr);
                }
            }
        }

        // Delete
        for (long long key : toUnload) {
            delete chunks[key];
            chunks.erase(key);
        }
    }
}

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
            chunk->setNeighbor(i, it->second);
            it->second->setNeighbor(i ^ 1, chunk);
        }
    }
}

void ChunkManager::render() {
    // **CRITICAL FIX**: Single lock, single iteration, render all block types
    std::lock_guard<std::mutex> lock(chunksMutex);
    for (auto& [_, chunk] : chunks) {
        chunk->render();  // Renders ALL meshes in one call
    }
}

void ChunkManager::renderType(BlockType type) {
    std::lock_guard<std::mutex> lock(chunksMutex);
    for (auto& [_, chunk] : chunks) {
        chunk->renderType(type);
    }
}

Block* ChunkManager::getBlockAt(int worldX, int worldY, int worldZ) {
    Block data = getBlockData(worldX, worldY, worldZ);
    return new Block(data.type);
}

Block ChunkManager::getBlockData(int worldX, int worldY, int worldZ) {
    int cx = worldX / CHUNK_SIZE_X; if (worldX < 0 && worldX % CHUNK_SIZE_X != 0) cx--;
    int cy = worldY / CHUNK_SIZE_Y; if (worldY < 0 && worldY % CHUNK_SIZE_Y != 0) cy--;
    int cz = worldZ / CHUNK_SIZE_Z; if (worldZ < 0 && worldZ % CHUNK_SIZE_Z != 0) cz--;

    int lx = worldX - cx * CHUNK_SIZE_X;
    int ly = worldY - cy * CHUNK_SIZE_Y;
    int lz = worldZ - cz * CHUNK_SIZE_Z;

    std::lock_guard<std::mutex> lock(chunksMutex);
    auto it = chunks.find(makeKey(cx, cy, cz));

    if (it == chunks.end()) return Block(Blocks::AIR);

    return it->second->getBlock(lx, ly, lz);
}

std::tuple<int, int, int> ChunkManager::worldToChunkCoords(float x, float y, float z) {
    int cx = static_cast<int>(std::floor(x / CHUNK_SIZE_X));
    int cy = static_cast<int>(std::floor(y / CHUNK_SIZE_Y));
    int cz = static_cast<int>(std::floor(-z / CHUNK_SIZE_Z));
    return { cx, cy, cz };
}

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

void ChunkManager::rebuildChunkMeshAt(int worldX, int worldY, int worldZ) {
    int chunkX = worldX / CHUNK_SIZE_X; if (worldX < 0 && worldX % CHUNK_SIZE_X != 0) chunkX--;
    int chunkY = worldY / CHUNK_SIZE_Y; if (worldY < 0 && worldY % CHUNK_SIZE_Y != 0) chunkY--;
    int chunkZ = worldZ / CHUNK_SIZE_Z; if (worldZ < 0 && worldZ % CHUNK_SIZE_Z != 0) chunkZ--;

    std::lock_guard<std::mutex> lock(chunksMutex);

    long long key = makeKey(chunkX, chunkY, chunkZ);
    auto it = chunks.find(key);
    if (it != chunks.end()) it->second->buildMesh(textureAtlas);

    static const int dx[6] = { 0,0,1,-1,0,0 };
    static const int dy[6] = { 0,0,0,0,1,-1 };
    static const int dz[6] = { 1,-1,0,0,0,0 };

    for (int dir = 0; dir < 6; dir++) {
        long long neighborKey = makeKey(chunkX + dx[dir], chunkY + dy[dir], chunkZ + dz[dir]);
        auto neighbor = chunks.find(neighborKey);
        if (neighbor != chunks.end()) neighbor->second->buildMesh(textureAtlas);
    }
}

std::vector<Chunk*> ChunkManager::getLoadedChunks() {
    std::vector<Chunk*> loaded;
    std::lock_guard<std::mutex> lock(chunksMutex);
    loaded.reserve(chunks.size());
    for (auto& pair : chunks) loaded.push_back(pair.second);
    return loaded;
}

void ChunkManager::setVerticalRenderDistance(int distance) {
    verticalRenderDistance = distance;
    std::cout << "Vertical render distance set to: " << distance << " chunks (" << (distance * 16) << " blocks)\n";
}

int ChunkManager::getVerticalRenderDistance() const {
    return verticalRenderDistance;
}