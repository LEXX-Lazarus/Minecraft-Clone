#include "ChunkManager.h"
#include <iostream>
#include <cmath>
#include <vector>

ChunkManager::ChunkManager(int renderDistance)
    : renderDistance(renderDistance), lastPlayerChunkX(INT_MAX), lastPlayerChunkZ(INT_MAX),
    currentRing(0), ringIndex(0) {
}

ChunkManager::~ChunkManager() {
    for (auto& pair : chunks) {
        delete pair.second;
    }
    chunks.clear();
}

// Convert world coordinates to chunk coordinates
std::pair<int, int> ChunkManager::worldToChunkCoords(float worldX, float worldZ) {
    int chunkX = static_cast<int>(std::floor(worldX / CHUNK_SIZE_X));
    int chunkZ = static_cast<int>(std::floor(-worldZ / CHUNK_SIZE_Z));
    return { chunkX, chunkZ };
}

// Prepare chunk rings around the player
void ChunkManager::prepareChunkLoadRings(int playerChunkX, int playerChunkZ) {
    chunkLoadRings.clear();
    currentRing = 0;
    ringIndex = 0;

    for (int r = 0; r <= renderDistance; r++) {
        std::vector<std::pair<int, int>> ringChunks;

        for (int dx = -r; dx <= r; dx++) {
            for (int dz = -r; dz <= r; dz++) {
                if (std::abs(dx) != r && std::abs(dz) != r) continue; // only the border of the ring
                int cx = playerChunkX + dx;
                int cz = playerChunkZ + dz;
                if (!isChunkLoaded(cx, cz))
                    ringChunks.push_back({ cx, cz });
            }
        }
        chunkLoadRings.push_back(ringChunks);
    }
}

// Main update loop: handle player movement and incremental chunk loading
void ChunkManager::update(float playerX, float playerZ) {
    auto [playerChunkX, playerChunkZ] = worldToChunkCoords(playerX, playerZ);

    // Player moved -> rebuild rings
    if (playerChunkX != lastPlayerChunkX || playerChunkZ != lastPlayerChunkZ) {
        std::cout << "Player moved to chunk (" << playerChunkX << ", " << playerChunkZ << ")" << std::endl;
        prepareChunkLoadRings(playerChunkX, playerChunkZ);
        unloadDistantChunks(playerChunkX, playerChunkZ);

        lastPlayerChunkX = playerChunkX;
        lastPlayerChunkZ = playerChunkZ;
    }

    // Load one chunk per update/frame from current ring
    if (currentRing < chunkLoadRings.size()) {
        auto& ring = chunkLoadRings[currentRing];
        if (ringIndex < ring.size()) {
            auto [cx, cz] = ring[ringIndex++];
            loadChunk(cx, cz);
        }
        else {
            // Ring done, move to next radius
            ringIndex = 0;
            currentRing++;
        }
    }
}

// Unload chunks beyond render distance
void ChunkManager::unloadDistantChunks(int playerChunkX, int playerChunkZ) {
    std::vector<std::pair<int, int>> chunksToUnload;

    for (auto& pair : chunks) {
        int chunkX = pair.first.first;
        int chunkZ = pair.first.second;

        int distX = std::abs(chunkX - playerChunkX);
        int distZ = std::abs(chunkZ - playerChunkZ);

        if (distX > renderDistance + 1 || distZ > renderDistance + 1) {
            chunksToUnload.push_back({ chunkX, chunkZ });
        }
    }

    for (auto& coords : chunksToUnload) {
        unloadChunk(coords.first, coords.second);
    }
}

// Check if a chunk is already loaded
bool ChunkManager::isChunkLoaded(int chunkX, int chunkZ) {
    return chunks.find({ chunkX, chunkZ }) != chunks.end();
}

// Link neighboring chunks for mesh consistency
void ChunkManager::linkChunkNeighbors(int chunkX, int chunkZ) {
    auto it = chunks.find({ chunkX, chunkZ });
    if (it == chunks.end()) return;

    Chunk* chunk = it->second;

    // North (Z+1)
    auto northIt = chunks.find({ chunkX, chunkZ + 1 });
    if (northIt != chunks.end()) {
        chunk->setNeighbor(0, northIt->second);
        northIt->second->setNeighbor(1, chunk);
    }
    // South (Z-1)
    auto southIt = chunks.find({ chunkX, chunkZ - 1 });
    if (southIt != chunks.end()) {
        chunk->setNeighbor(1, southIt->second);
        southIt->second->setNeighbor(0, chunk);
    }
    // East (X+1)
    auto eastIt = chunks.find({ chunkX + 1, chunkZ });
    if (eastIt != chunks.end()) {
        chunk->setNeighbor(2, eastIt->second);
        eastIt->second->setNeighbor(3, chunk);
    }
    // West (X-1)
    auto westIt = chunks.find({ chunkX - 1, chunkZ });
    if (westIt != chunks.end()) {
        chunk->setNeighbor(3, westIt->second);
        westIt->second->setNeighbor(2, chunk);
    }
}

// Load a single chunk, generate terrain, link neighbors, and build meshes
void ChunkManager::loadChunk(int chunkX, int chunkZ) {
    std::cout << "Loading chunk (" << chunkX << ", " << chunkZ << ")" << std::endl;

    Chunk* chunk = new Chunk(chunkX, chunkZ);
    TerrainGenerator::generateFlatTerrain(*chunk);

    chunks[{chunkX, chunkZ}] = chunk;

    // Link with neighbors
    linkChunkNeighbors(chunkX, chunkZ);

    // Build this chunk's mesh
    chunk->buildMesh();

    // Rebuild all surrounding neighbor meshes
    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++) {
            if (dx == 0 && dz == 0) continue;
            auto neighborIt = chunks.find({ chunkX + dx, chunkZ + dz });
            if (neighborIt != chunks.end()) {
                neighborIt->second->buildMesh();
            }
        }
    }
}

// Unload a single chunk
void ChunkManager::unloadChunk(int chunkX, int chunkZ) {
    std::cout << "Unloading chunk (" << chunkX << ", " << chunkZ << ")" << std::endl;

    auto it = chunks.find({ chunkX, chunkZ });
    if (it != chunks.end()) {
        delete it->second;
        chunks.erase(it);
    }
}

// Render all chunks
void ChunkManager::render() {
    for (auto& pair : chunks) {
        pair.second->render();
    }
}

// Render only blocks of a specific type
void ChunkManager::renderType(BlockType type) {
    for (auto& pair : chunks) {
        pair.second->renderType(type);
    }
}
