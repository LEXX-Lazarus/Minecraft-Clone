#include "ChunkManager.h"
#include <iostream>
#include <cmath>
#include <thread>
#include <chrono>
#include <algorithm>

ChunkManager::ChunkManager(int renderDistance)
    : renderDistance(renderDistance),
    lastPlayerChunkX(INT_MAX),
    lastPlayerChunkZ(INT_MAX),
    shouldStopGeneration(false)
{
    generationThread = std::make_unique<std::thread>(&ChunkManager::generationWorker, this);
}

ChunkManager::~ChunkManager() {
    shouldStopGeneration = true;
    if (generationThread && generationThread->joinable()) {
        generationThread->join();
    }

    for (auto& pair : chunks) {
        delete pair.second;
    }
    chunks.clear();
}

void ChunkManager::generationWorker() {
    while (!shouldStopGeneration) {
        std::pair<int, int> coords;
        bool hasWork = false;

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!generationQueue.empty()) {
                coords = generationQueue.front();
                generationQueue.pop();
                hasWork = true;
            }
        }

        if (hasWork) {
            Chunk* chunk = new Chunk(coords.first, coords.second);
            TerrainGenerator::generateFlatTerrain(*chunk);

            {
                std::lock_guard<std::mutex> lock(queueMutex);
                readyChunks.push(chunk);
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void ChunkManager::processReadyChunks() {
    // Process up to 3 chunks per frame
    for (int i = 0; i < 3; i++) {
        Chunk* chunk = nullptr;

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!readyChunks.empty()) {
                chunk = readyChunks.front();
                readyChunks.pop();
            }
        }

        if (!chunk) return;

        int chunkX = chunk->chunkX;
        int chunkZ = chunk->chunkZ;

        {
            std::lock_guard<std::mutex> lock(chunksMutex);
            chunks[{chunkX, chunkZ}] = chunk;
        }

        linkChunkNeighbors(chunkX, chunkZ);
        chunk->buildMesh();

        // Rebuild neighbor meshes
        for (auto [dx, dz] : std::vector<std::pair<int, int>>{ {0,1},{0,-1},{1,0},{-1,0} }) {
            std::lock_guard<std::mutex> lock(chunksMutex);
            auto it = chunks.find({ chunkX + dx, chunkZ + dz });
            if (it != chunks.end()) {
                it->second->buildMesh();
            }
        }
    }
}

std::pair<int, int> ChunkManager::worldToChunkCoords(float worldX, float worldZ) {
    int chunkX = static_cast<int>(std::floor(worldX / CHUNK_SIZE_X));
    int chunkZ = static_cast<int>(std::floor(-worldZ / CHUNK_SIZE_Z));
    return { chunkX, chunkZ };
}

void ChunkManager::updateChunksAroundPlayer(int playerChunkX, int playerChunkZ) {
    // Simple spiral pattern: load chunks in distance order
    std::vector<std::pair<int, int>> chunksToLoad;

    for (int dx = -renderDistance; dx <= renderDistance; dx++) {
        for (int dz = -renderDistance; dz <= renderDistance; dz++) {
            float distance = std::sqrt(dx * dx + dz * dz);
            if (distance <= renderDistance) {
                int cx = playerChunkX + dx;
                int cz = playerChunkZ + dz;

                if (!isChunkLoaded(cx, cz)) {
                    chunksToLoad.push_back({ cx, cz });
                }
            }
        }
    }

    // Sort by distance (closest first)
    std::sort(chunksToLoad.begin(), chunksToLoad.end(),
        [playerChunkX, playerChunkZ](const auto& a, const auto& b) {
            int dx1 = a.first - playerChunkX;
            int dz1 = a.second - playerChunkZ;
            int dx2 = b.first - playerChunkX;
            int dz2 = b.second - playerChunkZ;
            return (dx1 * dx1 + dz1 * dz1) < (dx2 * dx2 + dz2 * dz2);
        });

    // Queue closest chunks (limit to prevent overload)
    int maxToQueue = std::min(20, static_cast<int>(chunksToLoad.size()));
    for (int i = 0; i < maxToQueue; i++) {
        loadChunk(chunksToLoad[i].first, chunksToLoad[i].second);
    }
}

void ChunkManager::update(float playerX, float playerZ) {
    auto [playerChunkX, playerChunkZ] = worldToChunkCoords(playerX, playerZ);

    // Update when player moves to new chunk
    if (playerChunkX != lastPlayerChunkX || playerChunkZ != lastPlayerChunkZ) {
        std::cout << "Player at chunk (" << playerChunkX << ", " << playerChunkZ << ")" << std::endl;

        // Unload distant chunks
        unloadDistantChunks(playerChunkX, playerChunkZ);

        // Load new chunks
        updateChunksAroundPlayer(playerChunkX, playerChunkZ);

        lastPlayerChunkX = playerChunkX;
        lastPlayerChunkZ = playerChunkZ;
    }

    // Always process ready chunks
    processReadyChunks();
}

void ChunkManager::unloadDistantChunks(int playerChunkX, int playerChunkZ) {
    std::vector<std::pair<int, int>> toUnload;

    {
        std::lock_guard<std::mutex> lock(chunksMutex);
        for (auto& [coords, chunk] : chunks) {
            int dx = coords.first - playerChunkX;
            int dz = coords.second - playerChunkZ;
            float distance = std::sqrt(dx * dx + dz * dz);

            if (distance > renderDistance + 2) {
                toUnload.push_back(coords);
            }
        }
    }

    for (auto& [cx, cz] : toUnload) {
        unloadChunk(cx, cz);
    }

    if (!toUnload.empty()) {
        std::cout << "Unloaded " << toUnload.size() << " chunks" << std::endl;
    }
}

bool ChunkManager::isChunkLoaded(int chunkX, int chunkZ) {
    std::lock_guard<std::mutex> lock(chunksMutex);
    return chunks.find({ chunkX, chunkZ }) != chunks.end();
}

void ChunkManager::linkChunkNeighbors(int chunkX, int chunkZ) {
    Chunk* chunk = nullptr;
    {
        std::lock_guard<std::mutex> lock(chunksMutex);
        auto it = chunks.find({ chunkX, chunkZ });
        if (it == chunks.end()) return;
        chunk = it->second;
    }

    std::vector<std::pair<int, int>> directions{ {0,1},{0,-1},{1,0},{-1,0} };
    for (int i = 0; i < 4; ++i) {
        int nx = chunkX + directions[i].first;
        int nz = chunkZ + directions[i].second;

        std::lock_guard<std::mutex> lock(chunksMutex);
        auto neighborIt = chunks.find({ nx, nz });
        if (neighborIt != chunks.end()) {
            chunk->setNeighbor(i, neighborIt->second);
            neighborIt->second->setNeighbor(i ^ 1, chunk);
        }
    }
}

void ChunkManager::loadChunk(int chunkX, int chunkZ) {
    std::lock_guard<std::mutex> lock(queueMutex);
    generationQueue.push({ chunkX, chunkZ });
}

void ChunkManager::unloadChunk(int chunkX, int chunkZ) {
    std::lock_guard<std::mutex> lock(chunksMutex);
    auto it = chunks.find({ chunkX, chunkZ });
    if (it != chunks.end()) {
        delete it->second;
        chunks.erase(it);
    }
}

void ChunkManager::render() {
    std::lock_guard<std::mutex> lock(chunksMutex);
    for (auto& [coords, chunk] : chunks) {
        chunk->render();
    }
}

void ChunkManager::renderType(BlockType type) {
    std::lock_guard<std::mutex> lock(chunksMutex);
    for (auto& [coords, chunk] : chunks) {
        chunk->renderType(type);
    }
}

Block* ChunkManager::getBlockAt(int worldX, int worldY, int worldZ) {
    int chunkX = worldX / CHUNK_SIZE_X;
    if (worldX < 0 && worldX % CHUNK_SIZE_X != 0) chunkX--;

    int chunkZ = worldZ / CHUNK_SIZE_Z;
    if (worldZ < 0 && worldZ % CHUNK_SIZE_Z != 0) chunkZ--;

    int localX = worldX - chunkX * CHUNK_SIZE_X;
    int localZ = worldZ - chunkZ * CHUNK_SIZE_Z;

    std::lock_guard<std::mutex> lock(chunksMutex);
    auto it = chunks.find({ chunkX, chunkZ });
    if (it == chunks.end()) return nullptr;

    return new Block(it->second->getBlock(localX, worldY, localZ));
}