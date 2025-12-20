#ifndef CHUNK_MANAGER_H
#define CHUNK_MANAGER_H

#include "Chunk.h"
#include "TerrainGenerator.h"
#include <map>
#include <vector>
#include <utility>
#include <thread>
#include <mutex>
#include <queue>
#include <memory> // for std::unique_ptr

class ChunkManager {
public:
    ChunkManager(int renderDistance);
    ~ChunkManager();

    void update(float playerX, float playerZ);
    void render();
    void renderType(BlockType type);

    int getRenderDistance() const { return renderDistance; }

    Block* getBlockAt(int worldX, int worldY, int worldZ);

private:
    int renderDistance;
    std::map<std::pair<int, int>, Chunk*> chunks;

    int lastPlayerChunkX;
    int lastPlayerChunkZ;

    // Ring-based loading
    std::vector<std::vector<std::pair<int, int>>> chunkLoadRings;
    int currentRing;
    size_t ringIndex;

    // Async generation
    std::queue<std::pair<int, int>> generationQueue;
    std::queue<Chunk*> readyChunks;
    std::mutex queueMutex;
    std::mutex chunksMutex;
    std::unique_ptr<std::thread> generationThread; // <-- changed to unique_ptr
    bool shouldStopGeneration;

    void generationWorker();
    void processReadyChunks();

    std::pair<int, int> worldToChunkCoords(float worldX, float worldZ);
    void prepareChunkLoadRings(int playerChunkX, int playerChunkZ);
    void unloadDistantChunks(int playerChunkX, int playerChunkZ);
    bool isChunkLoaded(int chunkX, int chunkZ);
    void loadChunk(int chunkX, int chunkZ);
    void unloadChunk(int chunkX, int chunkZ);
    void linkChunkNeighbors(int chunkX, int chunkZ);
};

#endif
