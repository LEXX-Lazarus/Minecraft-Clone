#include "ChunkManager.h"
#include <iostream>
#include <cmath>

ChunkManager::ChunkManager(int renderDistance)
    : renderDistance(renderDistance), lastPlayerChunkX(INT_MAX), lastPlayerChunkZ(INT_MAX),
    currentRing(0), ringIndex(0), shouldStopGeneration(false) {

    // Start background generation thread
    generationThread = new std::thread(&ChunkManager::generationWorker, this);
}

ChunkManager::~ChunkManager() {
    // Stop generation thread
    shouldStopGeneration = true;
    if (generationThread) {
        generationThread->join();
        delete generationThread;
    }

    for (auto& pair : chunks) {
        delete pair.second;
    }
    chunks.clear();
}

// Background thread that generates chunks
void ChunkManager::generationWorker() {
    while (!shouldStopGeneration) {
        std::pair<int, int> coords;
        bool hasWork = false;

        // Check if there's work to do
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!generationQueue.empty()) {
                coords = generationQueue.front();
                generationQueue.pop();
                hasWork = true;
            }
        }

        if (hasWork) {
            // Generate chunk (this is the slow part)
            Chunk* chunk = new Chunk(coords.first, coords.second);
            TerrainGenerator::generateFlatTerrain(*chunk);

            // Add to ready queue
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                readyChunks.push(chunk);
            }
        }
        else {
            // No work, sleep briefly
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

// Process chunks that finished generating (called from main thread)
void ChunkManager::processReadyChunks() {
    Chunk* chunk = nullptr;

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (!readyChunks.empty()) {
            chunk = readyChunks.front();
            readyChunks.pop();
        }
    }

    if (chunk) {
        int chunkX = chunk->chunkX;
        int chunkZ = chunk->chunkZ;

        std::cout << "Finalizing chunk (" << chunkX << ", " << chunkZ << ")" << std::endl;

        {
            std::lock_guard<std::mutex> lock(chunksMutex);
            chunks[{chunkX, chunkZ}] = chunk;
        }

        // Link neighbors
        linkChunkNeighbors(chunkX, chunkZ);

        // Build mesh
        chunk->buildMesh();

        // Rebuild cardinal neighbors
        auto northIt = chunks.find({ chunkX, chunkZ + 1 });
        if (northIt != chunks.end()) northIt->second->buildMesh();

        auto southIt = chunks.find({ chunkX, chunkZ - 1 });
        if (southIt != chunks.end()) southIt->second->buildMesh();

        auto eastIt = chunks.find({ chunkX + 1, chunkZ });
        if (eastIt != chunks.end()) eastIt->second->buildMesh();

        auto westIt = chunks.find({ chunkX - 1, chunkZ });
        if (westIt != chunks.end()) westIt->second->buildMesh();
    }
}

std::pair<int, int> ChunkManager::worldToChunkCoords(float worldX, float worldZ) {
    int chunkX = static_cast<int>(std::floor(worldX / CHUNK_SIZE_X));
    int chunkZ = static_cast<int>(std::floor(-worldZ / CHUNK_SIZE_Z));
    return { chunkX, chunkZ };
}

void ChunkManager::prepareChunkLoadRings(int playerChunkX, int playerChunkZ) {
    chunkLoadRings.clear();
    currentRing = 0;
    ringIndex = 0;

    for (int r = 0; r <= renderDistance; r++) {
        std::vector<std::pair<int, int>> ringChunks;

        for (int dx = -r; dx <= r; dx++) {
            for (int dz = -r; dz <= r; dz++) {
                if (std::abs(dx) != r && std::abs(dz) != r) continue;
                int cx = playerChunkX + dx;
                int cz = playerChunkZ + dz;
                if (!isChunkLoaded(cx, cz))
                    ringChunks.push_back({ cx, cz });
            }
        }
        chunkLoadRings.push_back(ringChunks);
    }
}

void ChunkManager::update(float playerX, float playerZ) {
    auto [playerChunkX, playerChunkZ] = worldToChunkCoords(playerX, playerZ);

    if (playerChunkX != lastPlayerChunkX || playerChunkZ != lastPlayerChunkZ) {
        std::cout << "Player moved to chunk (" << playerChunkX << ", " << playerChunkZ << ")" << std::endl;
        prepareChunkLoadRings(playerChunkX, playerChunkZ);
        unloadDistantChunks(playerChunkX, playerChunkZ);

        lastPlayerChunkX = playerChunkX;
        lastPlayerChunkZ = playerChunkZ;
    }

    // Queue chunks for generation
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

    // Process any ready chunks (limit to 1 per frame for stable FPS)
    processReadyChunks();
}

void ChunkManager::unloadDistantChunks(int playerChunkX, int playerChunkZ) {
    std::vector<std::pair<int, int>> chunksToUnload;

    {
        std::lock_guard<std::mutex> lock(chunksMutex);
        for (auto& pair : chunks) {
            int chunkX = pair.first.first;
            int chunkZ = pair.first.second;

            int distX = std::abs(chunkX - playerChunkX);
            int distZ = std::abs(chunkZ - playerChunkZ);

            if (distX > renderDistance + 1 || distZ > renderDistance + 1) {
                chunksToUnload.push_back({ chunkX, chunkZ });
            }
        }
    }

    for (auto& coords : chunksToUnload) {
        unloadChunk(coords.first, coords.second);
    }
}

bool ChunkManager::isChunkLoaded(int chunkX, int chunkZ) {
    std::lock_guard<std::mutex> lock(chunksMutex);
    return chunks.find({ chunkX, chunkZ }) != chunks.end();
}

void ChunkManager::linkChunkNeighbors(int chunkX, int chunkZ) {
    auto it = chunks.find({ chunkX, chunkZ });
    if (it == chunks.end()) return;

    Chunk* chunk = it->second;

    auto northIt = chunks.find({ chunkX, chunkZ + 1 });
    if (northIt != chunks.end()) {
        chunk->setNeighbor(0, northIt->second);
        northIt->second->setNeighbor(1, chunk);
    }

    auto southIt = chunks.find({ chunkX, chunkZ - 1 });
    if (southIt != chunks.end()) {
        chunk->setNeighbor(1, southIt->second);
        southIt->second->setNeighbor(0, chunk);
    }

    auto eastIt = chunks.find({ chunkX + 1, chunkZ });
    if (eastIt != chunks.end()) {
        chunk->setNeighbor(2, eastIt->second);
        eastIt->second->setNeighbor(3, chunk);
    }

    auto westIt = chunks.find({ chunkX - 1, chunkZ });
    if (westIt != chunks.end()) {
        chunk->setNeighbor(3, westIt->second);
        westIt->second->setNeighbor(2, chunk);
    }
}

void ChunkManager::loadChunk(int chunkX, int chunkZ) {
    // Add to generation queue instead of generating immediately
    std::lock_guard<std::mutex> lock(queueMutex);
    generationQueue.push({ chunkX, chunkZ });
}

void ChunkManager::unloadChunk(int chunkX, int chunkZ) {
    std::cout << "Unloading chunk (" << chunkX << ", " << chunkZ << ")" << std::endl;

    std::lock_guard<std::mutex> lock(chunksMutex);
    auto it = chunks.find({ chunkX, chunkZ });
    if (it != chunks.end()) {
        delete it->second;
        chunks.erase(it);
    }
}

void ChunkManager::render() {
    std::lock_guard<std::mutex> lock(chunksMutex);
    for (auto& pair : chunks) {
        pair.second->render();
    }
}

void ChunkManager::renderType(BlockType type) {
    std::lock_guard<std::mutex> lock(chunksMutex);
    for (auto& pair : chunks) {
        pair.second->renderType(type);
    }
}

Block* ChunkManager::getBlockAt(int worldX, int worldY, int worldZ) {
    // Convert world coordinates to chunk coordinates
    int chunkX = worldX / CHUNK_SIZE_X;
    if (worldX < 0 && worldX % CHUNK_SIZE_X != 0) chunkX--;

    int chunkZ = worldZ / CHUNK_SIZE_Z;
    if (worldZ < 0 && worldZ % CHUNK_SIZE_Z != 0) chunkZ--;

    // Get local coordinates within chunk
    int localX = worldX - (chunkX * CHUNK_SIZE_X);
    int localZ = worldZ - (chunkZ * CHUNK_SIZE_Z);

    // Check if chunk exists
    std::lock_guard<std::mutex> lock(chunksMutex);
    auto it = chunks.find({ chunkX, chunkZ });
    if (it == chunks.end()) {
        return nullptr;  // Chunk not loaded
    }

    // Get block from chunk
    return new Block(it->second->getBlock(localX, worldY, localZ));
}