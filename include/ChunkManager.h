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
#include <memory>

// LOD (Level of Detail) levels
enum class ChunkLOD {
    FULL = 0,      // Full detail - entities, all blocks
    MEDIUM = 1,    // Medium detail - simplified mesh
    LOW = 2        // Low detail - very simplified, distant chunks
};

class ChunkManager {
public:
    ChunkManager(int renderDistance);
    ~ChunkManager();

    void update(float playerX, float playerZ);
    void render();
    void renderType(BlockType type);

    int getRenderDistance() const { return renderDistance; }
    Block* getBlockAt(int worldX, int worldY, int worldZ);

    // LOD settings
    void setLODDistances(float fullDetail, float mediumDetail, float lowDetail);

private:
    int renderDistance;
    std::map<std::pair<int, int>, Chunk*> chunks;

    // LOD distance thresholds (in chunks)
    float fullDetailDistance;
    float mediumDetailDistance;
    float lowDetailDistance;

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
    std::unique_ptr<std::thread> generationThread;
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

    // LOD helpers
    ChunkLOD calculateLOD(int chunkX, int chunkZ, int playerChunkX, int playerChunkZ);
    void renderChunkAtLOD(Chunk* chunk, ChunkLOD lod, BlockType type);
};

#endif