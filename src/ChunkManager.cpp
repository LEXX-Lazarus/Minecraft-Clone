#include "ChunkManager.h"
#include <iostream>
#include <cmath>
#include <thread>
#include <chrono>
#include <mutex>
#include <queue>
#include <vector>
#include <map>
#include <algorithm>

ChunkManager::ChunkManager(int renderDistance)
    : renderDistance(renderDistance), lastPlayerChunkX(INT_MAX), lastPlayerChunkZ(INT_MAX),
    currentRing(0), ringIndex(0), shouldStopGeneration(false),
    fullDetailDistance(8.0f),      // Full detail within 8 chunks
    mediumDetailDistance(16.0f),   // Medium detail 8-16 chunks
    lowDetailDistance(30.0f)       // Low detail 16-24 chunks
{
    generationThread = std::make_unique<std::thread>(&ChunkManager::generationWorker, this);
}

ChunkManager::~ChunkManager() {
    // Stop generation thread
    shouldStopGeneration = true;
    if (generationThread && generationThread->joinable()) {
        generationThread->join();
    }

    for (auto& pair : chunks) {
        delete pair.second;
    }
    chunks.clear();
}

// Background worker thread that generates chunks asynchronously
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
            // Generate chunk terrain
            Chunk* chunk = new Chunk(coords.first, coords.second);
            TerrainGenerator::generateFlatTerrain(*chunk);

            // Push to ready queue
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

// Process ready chunks (called from main thread)
void ChunkManager::processReadyChunks() {
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

    std::cout << "Finalizing chunk (" << chunkX << ", " << chunkZ << ")\n";

    {
        std::lock_guard<std::mutex> lock(chunksMutex);
        chunks[{chunkX, chunkZ}] = chunk;
    }

    linkChunkNeighbors(chunkX, chunkZ);

    // Build mesh for this chunk and its cardinal neighbors
    chunk->buildMesh();
    for (auto [dx, dz] : std::vector<std::pair<int, int>>{ {0,1},{0,-1},{1,0},{-1,0} }) {
        std::lock_guard<std::mutex> lock(chunksMutex);
        auto it = chunks.find({ chunkX + dx, chunkZ + dz });
        if (it != chunks.end()) it->second->buildMesh();
    }
}

// Convert world coordinates to chunk coordinates
std::pair<int, int> ChunkManager::worldToChunkCoords(float worldX, float worldZ) {
    int chunkX = static_cast<int>(std::floor(worldX / CHUNK_SIZE_X));
    int chunkZ = static_cast<int>(std::floor(-worldZ / CHUNK_SIZE_Z)); // inverted Z
    return { chunkX, chunkZ };
}

// Precompute chunk load rings around player for gradual loading (CIRCULAR)
void ChunkManager::prepareChunkLoadRings(int playerChunkX, int playerChunkZ) {
    chunkLoadRings.clear();
    currentRing = 0;
    ringIndex = 0;

    // Load chunks up to lowDetailDistance (furthest LOD)
    int maxDistance = static_cast<int>(lowDetailDistance);

    for (int r = 0; r <= maxDistance; r++) {
        std::vector<std::pair<int, int>> ringChunks;

        for (int dx = -maxDistance; dx <= maxDistance; dx++) {
            for (int dz = -maxDistance; dz <= maxDistance; dz++) {
                float distance = std::sqrt(dx * dx + dz * dz);

                if (distance > r + 0.5f || distance < r - 0.5f) continue;
                if (distance > maxDistance) continue;

                int cx = playerChunkX + dx;
                int cz = playerChunkZ + dz;

                if (!isChunkLoaded(cx, cz)) {
                    ringChunks.push_back({ cx, cz });
                }
            }
        }

        std::sort(ringChunks.begin(), ringChunks.end(),
            [playerChunkX, playerChunkZ](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                int dx1 = a.first - playerChunkX;
                int dz1 = a.second - playerChunkZ;
                int dx2 = b.first - playerChunkX;
                int dz2 = b.second - playerChunkZ;
                return (dx1 * dx1 + dz1 * dz1) < (dx2 * dx2 + dz2 * dz2);
            });

        if (!ringChunks.empty()) {
            chunkLoadRings.push_back(ringChunks);
        }
    }
}

// Main update function, called every frame
void ChunkManager::update(float playerX, float playerZ) {
    auto [playerChunkX, playerChunkZ] = worldToChunkCoords(playerX, playerZ);

    if (playerChunkX != lastPlayerChunkX || playerChunkZ != lastPlayerChunkZ) {
        std::cout << "Player moved to chunk (" << playerChunkX << ", " << playerChunkZ << ")\n";
        prepareChunkLoadRings(playerChunkX, playerChunkZ);
        unloadDistantChunks(playerChunkX, playerChunkZ);

        lastPlayerChunkX = playerChunkX;
        lastPlayerChunkZ = playerChunkZ;
    }

    // Queue chunks from the current ring
    if (currentRing < chunkLoadRings.size()) {
        auto& ring = chunkLoadRings[currentRing];
        if (ringIndex < ring.size()) {
            auto [cx, cz] = ring[ringIndex++];
            loadChunk(cx, cz);
        }
        else {
            ringIndex = 0;
            currentRing++;
        }
    }

    // Limit ready chunks processed per frame to maintain FPS
    processReadyChunks();
}

// Remove chunks beyond render distance (CIRCULAR)
void ChunkManager::unloadDistantChunks(int playerChunkX, int playerChunkZ) {
    std::vector<std::pair<int, int>> toUnload;

    {
        std::lock_guard<std::mutex> lock(chunksMutex);
        for (auto& [coords, chunk] : chunks) {
            int dx = coords.first - playerChunkX;
            int dz = coords.second - playerChunkZ;
            float distance = std::sqrt(dx * dx + dz * dz);

            // Unload chunks beyond low detail distance
            if (distance > lowDetailDistance + 2) {
                toUnload.push_back(coords);
            }
        }
    }

    for (auto& [cx, cz] : toUnload) unloadChunk(cx, cz);
}

// Check if a chunk is already loaded
bool ChunkManager::isChunkLoaded(int chunkX, int chunkZ) {
    std::lock_guard<std::mutex> lock(chunksMutex);
    return chunks.find({ chunkX, chunkZ }) != chunks.end();
}

// Link neighbors for smooth mesh updates
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
            neighborIt->second->setNeighbor(i ^ 1, chunk); // 0<->1, 2<->3
        }
    }
}

// Queue chunk for generation
void ChunkManager::loadChunk(int chunkX, int chunkZ) {
    std::lock_guard<std::mutex> lock(queueMutex);
    generationQueue.push({ chunkX, chunkZ });
}

// Delete chunk from memory
void ChunkManager::unloadChunk(int chunkX, int chunkZ) {
    std::cout << "Unloading chunk (" << chunkX << ", " << chunkZ << ")\n";
    std::lock_guard<std::mutex> lock(chunksMutex);
    auto it = chunks.find({ chunkX, chunkZ });
    if (it != chunks.end()) {
        delete it->second;
        chunks.erase(it);
    }
}

// Render all chunks
void ChunkManager::render() {
    std::lock_guard<std::mutex> lock(chunksMutex);
    for (auto& [coords, chunk] : chunks) chunk->render();
}

// Render only specific block type
void ChunkManager::renderType(BlockType type) {
    std::lock_guard<std::mutex> lock(chunksMutex);

    for (auto& [coords, chunk] : chunks) {
        ChunkLOD lod = calculateLOD(coords.first, coords.second,
            lastPlayerChunkX, lastPlayerChunkZ);
        renderChunkAtLOD(chunk, lod, type);
    }
}

// Get block at world coordinates (returns nullptr if chunk not loaded)
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

void ChunkManager::setLODDistances(float fullDetail, float mediumDetail, float lowDetail) {
    fullDetailDistance = fullDetail;
    mediumDetailDistance = mediumDetail;
    lowDetailDistance = lowDetail;
}

ChunkLOD ChunkManager::calculateLOD(int chunkX, int chunkZ, int playerChunkX, int playerChunkZ) {
    int dx = chunkX - playerChunkX;
    int dz = chunkZ - playerChunkZ;
    float distance = std::sqrt(dx * dx + dz * dz);

    if (distance <= fullDetailDistance) {
        return ChunkLOD::FULL;
    }
    else if (distance <= mediumDetailDistance) {
        return ChunkLOD::MEDIUM;
    }
    else {
        return ChunkLOD::LOW;
    }
}

void ChunkManager::renderChunkAtLOD(Chunk* chunk, ChunkLOD lod, BlockType type) {
    if (lod == ChunkLOD::FULL) {
        // Full detail - render everything normally
        chunk->renderType(type);
    }
    else if (lod == ChunkLOD::MEDIUM) {
        // Medium detail - skip every other block vertically for performance
        // Still render all horizontal blocks
        // For now, just render normally but could optimize mesh
        chunk->renderType(type);
    }
    else if (lod == ChunkLOD::LOW) {
        // Low detail - only render surface blocks
        // Very simplified mesh for distant chunks
        chunk->renderType(type);
    }
}