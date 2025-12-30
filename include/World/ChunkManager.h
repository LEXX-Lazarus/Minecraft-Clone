#ifndef CHUNKMANAGER_H
#define CHUNKMANAGER_H

#include "World/Chunk.h"
#include "World/TerrainGenerator.h"
#include "WorldSave.h"
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <tuple>
#include <vector>
#include <climits>

// Hash for std::tuple<int,int,int> for 3D chunk coordinates
struct tuple_hash {
    std::size_t operator()(const std::tuple<int, int, int>& t) const noexcept {
        auto h1 = std::hash<int>()(std::get<0>(t));
        auto h2 = std::hash<int>()(std::get<1>(t));
        auto h3 = std::hash<int>()(std::get<2>(t));
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

struct ChunkDistanceEntry {
    int x;
    int y;
    int z;
    int distSq;
};

class ChunkManager {
public:
    ChunkManager(int renderDistance, const std::string& worldName = "world1");
    ~ChunkManager();

    void update(float playerX, float playerY, float playerZ);
    void render();
    void renderType(BlockType type);

    // LEGACY: Returns pointer to heap memory (Fixes your compilation errors in external files)
    Block* getBlockAt(int worldX, int worldY, int worldZ);

    // OPTIMIZED: Returns block by value (Used by Player to fix lag)
    Block getBlockData(int worldX, int worldY, int worldZ);

    std::tuple<int, int, int> worldToChunkCoords(float x, float y, float z);
    bool setBlockAt(int worldX, int worldY, int worldZ, BlockType type);
    void rebuildChunkMeshAt(int worldX, int worldY, int worldZ);

    std::vector<Chunk*> getLoadedChunks();

private:
    void generationWorker();
    void processReadyChunks();
    void updateDesiredChunks(int pcx, int pcy, int pcz);
    void unloadDistantChunks(int pcx, int pcy, int pcz);
    void linkChunkNeighbors(Chunk* chunk);

    bool isChunkLoaded(int cx, int cy, int cz);
    void loadChunk(int cx, int cy, int cz);
    void unloadChunk(int cx, int cy, int cz);

    long long makeKey(int x, int y, int z) const;

    std::unique_ptr<WorldSave> worldSave;

    int renderDistance;
    int renderDistanceSquared;

    int lastPlayerChunkX;
    int lastPlayerChunkY;
    int lastPlayerChunkZ;

    std::unordered_map<long long, Chunk*> chunks;
    std::mutex chunksMutex;

    std::unordered_set<std::tuple<int, int, int>, tuple_hash> chunksBeingGenerated;
    std::unordered_set<long long> queuedChunks;
    std::queue<std::tuple<int, int, int>> generationQueue;
    std::queue<Chunk*> readyChunks;

    std::mutex mutex;
    std::condition_variable queueCV;
    std::vector<std::thread> workerThreads; // Changed from single thread
    bool shouldStop;
};

#endif