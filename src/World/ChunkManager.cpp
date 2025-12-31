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

        std::vector<ModifiedBlock> modifications;
        worldSave->loadChunkModifications(cx, cz, modifications);

        int chunkWorldYMin = cy * CHUNK_SIZE_Y;
        int chunkWorldYMax = chunkWorldYMin + CHUNK_SIZE_Y - 1;

        for (auto& mod : modifications) {
            if (mod.y < chunkWorldYMin || mod.y > chunkWorldYMax) {
                continue;
            }

            int localX = mod.x - cx * CHUNK_SIZE_X;
            int localY = mod.y - cy * CHUNK_SIZE_Y;
            int localZ = mod.z - cz * CHUNK_SIZE_Z;

            if (localX >= 0 && localX < CHUNK_SIZE_X &&
                localY >= 0 && localY < CHUNK_SIZE_Y &&
                localZ >= 0 && localZ < CHUNK_SIZE_Z) {
                chunk->setBlock(localX, localY, localZ, mod.type);
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

    for (int dx = -renderDistance; dx <= renderDistance; dx++) {
        for (int dz = -renderDistance; dz <= renderDistance; dz++) {

            int distSqXZ = dx * dx + dz * dz;
            if (distSqXZ > renderDistanceSquared) continue;

            for (int dy = -verticalRenderDistance; dy <= verticalRenderDistance; dy++) {
                int cx = pcx + dx;
                int cy = pcy + dy;
                int cz = pcz + dz;

                if (cy < 0 || cy >= 6250) continue;

                ordered.push_back({ cx, cy, cz, distSqXZ });
            }
        }
    }

    std::sort(ordered.begin(), ordered.end(), [](const ChunkDistanceEntry& a, const ChunkDistanceEntry& b) {
        return a.distSq < b.distSq;
        });

    unloadDistantChunks(pcx, pcy, pcz);

    int queued = 0;
    for (const auto& entry : ordered) {
        long long key = makeKey(entry.x, entry.y, entry.z);

        bool skip = false;
        {
            std::lock_guard<std::mutex> chunkLock(chunksMutex);
            if (chunks.count(key)) skip = true;
        }
        if (skip) continue;

        {
            std::lock_guard<std::mutex> lock(mutex);
            if (queuedChunks.count(key)) continue;

            queuedChunks.insert(key);
            generationQueue.push({ entry.x, entry.y, entry.z });
            queued++;
        }
    }

    if (queued > 0) {
        queueCV.notify_all();
    }
}

void ChunkManager::processReadyChunks() {
    int readyCount = 0;
    {
        std::lock_guard<std::mutex> lock(mutex);
        readyCount = readyChunks.size();
    }

    int maxPerFrame = 24;
    if (readyCount > 50) maxPerFrame = 48;
    if (readyCount > 100) maxPerFrame = 96;
    if (readyCount > 200) maxPerFrame = 144;

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

        std::lock_guard<std::mutex> cLock(chunksMutex);

        if (chunks.count(key)) {
            delete chunk;
            continue;
        }
        chunks[key] = chunk;

        linkChunkNeighborsUnsafe(chunk);

        chunk->buildMesh(textureAtlas);

        static const int dx[6] = { 0, 0, 1, -1, 0, 0 };
        static const int dy[6] = { 0, 0, 0, 0, 1, -1 };
        static const int dz[6] = { 1, -1, 0, 0, 0, 0 };

        for (int dir = 0; dir < 6; dir++) {
            Chunk* neighbor = chunk->getNeighbor(dir);
            if (neighbor && neighbor->getNeighbor(dir ^ 1) == chunk) {
                neighbor->buildMesh(textureAtlas);
            }
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
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
            Chunk* chunk = chunks[key];

            for (int dir = 0; dir < 6; dir++) {
                Chunk* neighbor = chunk->getNeighbor(dir);
                if (neighbor) {
                    neighbor->setNeighbor(dir ^ 1, nullptr);
                }
            }

            delete chunk;
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