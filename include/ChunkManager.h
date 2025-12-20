#ifndef CHUNK_MANAGER_H
#define CHUNK_MANAGER_H

#include "Chunk.h"
#include "TerrainGenerator.h"
#include <map>
#include <vector>
#include <utility>

class ChunkManager {
public:
    ChunkManager(int renderDistance);
    ~ChunkManager();

    void update(float playerX, float playerZ);
    void render();
    void renderType(BlockType type);

    int getRenderDistance() const { return renderDistance; }

private:
    int renderDistance;
    std::map<std::pair<int, int>, Chunk*> chunks;

    int lastPlayerChunkX;
    int lastPlayerChunkZ;

    // --- New members for radius-by-radius incremental loading ---
    std::vector<std::vector<std::pair<int, int>>> chunkLoadRings;
    size_t currentRing = 0;
    size_t ringIndex = 0;

    // --- Internal helper functions ---
    std::pair<int, int> worldToChunkCoords(float worldX, float worldZ);
    void loadChunksAroundPlayer(int playerChunkX, int playerChunkZ); // optional, still can keep
    void unloadDistantChunks(int playerChunkX, int playerChunkZ);
    bool isChunkLoaded(int chunkX, int chunkZ);
    void loadChunk(int chunkX, int chunkZ);
    void unloadChunk(int chunkX, int chunkZ);
    void linkChunkNeighbors(int chunkX, int chunkZ);

    // New helper to prepare chunk rings
    void prepareChunkLoadRings(int playerChunkX, int playerChunkZ);
};

#endif
