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

    std::pair<int, int> worldToChunkCoords(float worldX, float worldZ);
    void loadChunksAroundPlayer(int playerChunkX, int playerChunkZ);
    void unloadDistantChunks(int playerChunkX, int playerChunkZ);
    bool isChunkLoaded(int chunkX, int chunkZ);
    void loadChunk(int chunkX, int chunkZ);
    void unloadChunk(int chunkX, int chunkZ);
    void linkChunkNeighbors(int chunkX, int chunkZ);  // NEW
};

#endif