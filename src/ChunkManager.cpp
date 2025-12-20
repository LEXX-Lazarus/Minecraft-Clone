#include "ChunkManager.h"
#include <iostream>
#include <cmath>

ChunkManager::ChunkManager(int renderDistance)
    : renderDistance(renderDistance), lastPlayerChunkX(INT_MAX), lastPlayerChunkZ(INT_MAX) {
}

ChunkManager::~ChunkManager() {
    for (auto& pair : chunks) {
        delete pair.second;
    }
    chunks.clear();
}

std::pair<int, int> ChunkManager::worldToChunkCoords(float worldX, float worldZ) {
    int chunkX = static_cast<int>(std::floor(worldX / CHUNK_SIZE_X));
    int chunkZ = static_cast<int>(std::floor(-worldZ / CHUNK_SIZE_Z));
    return { chunkX, chunkZ };
}

void ChunkManager::update(float playerX, float playerZ) {
    auto [playerChunkX, playerChunkZ] = worldToChunkCoords(playerX, playerZ);

    if (playerChunkX != lastPlayerChunkX || playerChunkZ != lastPlayerChunkZ) {
        std::cout << "Player moved to chunk (" << playerChunkX << ", " << playerChunkZ << ")" << std::endl;

        loadChunksAroundPlayer(playerChunkX, playerChunkZ);
        unloadDistantChunks(playerChunkX, playerChunkZ);

        lastPlayerChunkX = playerChunkX;
        lastPlayerChunkZ = playerChunkZ;
    }
}

void ChunkManager::loadChunksAroundPlayer(int playerChunkX, int playerChunkZ) {
    for (int x = playerChunkX - renderDistance; x <= playerChunkX + renderDistance; x++) {
        for (int z = playerChunkZ - renderDistance; z <= playerChunkZ + renderDistance; z++) {
            if (!isChunkLoaded(x, z)) {
                loadChunk(x, z);
            }
        }
    }
}

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

bool ChunkManager::isChunkLoaded(int chunkX, int chunkZ) {
    return chunks.find({ chunkX, chunkZ }) != chunks.end();
}

void ChunkManager::linkChunkNeighbors(int chunkX, int chunkZ) {
    auto it = chunks.find({ chunkX, chunkZ });
    if (it == chunks.end()) return;

    Chunk* chunk = it->second;

    // Link North neighbor (Z+1)
    auto northIt = chunks.find({ chunkX, chunkZ + 1 });
    if (northIt != chunks.end()) {
        chunk->setNeighbor(0, northIt->second);
        northIt->second->setNeighbor(1, chunk);
    }

    // Link South neighbor (Z-1)
    auto southIt = chunks.find({ chunkX, chunkZ - 1 });
    if (southIt != chunks.end()) {
        chunk->setNeighbor(1, southIt->second);
        southIt->second->setNeighbor(0, chunk);
    }

    // Link East neighbor (X+1)
    auto eastIt = chunks.find({ chunkX + 1, chunkZ });
    if (eastIt != chunks.end()) {
        chunk->setNeighbor(2, eastIt->second);
        eastIt->second->setNeighbor(3, chunk);
    }

    // Link West neighbor (X-1)
    auto westIt = chunks.find({ chunkX - 1, chunkZ });
    if (westIt != chunks.end()) {
        chunk->setNeighbor(3, westIt->second);
        westIt->second->setNeighbor(2, chunk);
    }
}

void ChunkManager::loadChunk(int chunkX, int chunkZ) {
    std::cout << "Loading chunk (" << chunkX << ", " << chunkZ << ")" << std::endl;

    Chunk* chunk = new Chunk(chunkX, chunkZ);
    TerrainGenerator::generateFlatTerrain(*chunk);

    chunks[{chunkX, chunkZ}] = chunk;

    // Link with neighbors
    linkChunkNeighbors(chunkX, chunkZ);

    // Build mesh after neighbors are linked
    chunk->buildMesh();

    // IMPORTANT: Rebuild neighbor meshes so they can see this chunk's blocks
    auto northIt = chunks.find({ chunkX, chunkZ + 1 });
    if (northIt != chunks.end()) northIt->second->buildMesh();

    auto southIt = chunks.find({ chunkX, chunkZ - 1 });
    if (southIt != chunks.end()) southIt->second->buildMesh();

    auto eastIt = chunks.find({ chunkX + 1, chunkZ });
    if (eastIt != chunks.end()) eastIt->second->buildMesh();

    auto westIt = chunks.find({ chunkX - 1, chunkZ });
    if (westIt != chunks.end()) westIt->second->buildMesh();
}

void ChunkManager::unloadChunk(int chunkX, int chunkZ) {
    std::cout << "Unloading chunk (" << chunkX << ", " << chunkZ << ")" << std::endl;

    auto it = chunks.find({ chunkX, chunkZ });
    if (it != chunks.end()) {
        delete it->second;
        chunks.erase(it);
    }
}

void ChunkManager::render() {
    for (auto& pair : chunks) {
        pair.second->render();
    }
}

void ChunkManager::renderType(BlockType type) {
    for (auto& pair : chunks) {
        pair.second->renderType(type);
    }
}