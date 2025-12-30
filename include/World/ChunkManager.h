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

    Block* getBlockAt(int worldX, int worldY, int worldZ);
    Block getBlockData(int worldX, int worldY, int worldZ);  // OPTIMIZED: No heap allocation
    std::tuple<int, int, int> worldToChunkCoords(float x, float y, float z);

    bool setBlockAt(int worldX, int worldY, int worldZ, BlockType type);
    void rebuildChunkMeshAt(int worldX, int worldY, int worldZ);

    std::vector<Chunk*> getLoadedChunks();

    // NEW: Control vertical render distance separately
    void setVerticalRenderDistance(int distance);
    int getVerticalRenderDistance() const;

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

    int renderDistance;         // XZ plane radius (horizontal)
    int renderDistanceSquared;
    int verticalRenderDistance; // Y axis range (up/down from player)

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
    std::vector<std::thread> workerThreads;  // Multiple worker threads
    bool shouldStop;
};

#endif