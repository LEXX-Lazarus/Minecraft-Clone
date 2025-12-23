#pragma once
#include "Chunk.h"
#include "TerrainGenerator.h"
#include "WorldSave.h"
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <climits>
#include <cmath>
#include <vector>

// Hash for std::pair<int,int> to use in unordered_set/map
struct pair_hash {
    std::size_t operator()(const std::pair<int, int>& p) const noexcept {
        return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
    }
};

struct ChunkDistanceEntry {
    int x;
    int z;
    int distSq;
};

class ChunkManager {
public:
    ChunkManager(int renderDistance, const std::string& worldName = "world1");
    ~ChunkManager();

    void update(float playerX, float playerZ);
    void render();
    void renderType(BlockType type);
    Block* getBlockAt(int worldX, int worldY, int worldZ);
    std::pair<int, int> worldToChunkCoords(float x, float z);

    // Block modification methods
    bool setBlockAt(int worldX, int worldY, int worldZ, BlockType type);
    void rebuildChunkMeshAt(int worldX, int worldY, int worldZ);

private:
    // =============================
    // Threaded generation
    // =============================
    void generationWorker();
    void processReadyChunks();

    // =============================
    // Chunk management
    // =============================
    void updateDesiredChunks(int pcx, int pcz);
    void unloadDistantChunks(int pcx, int pcz);
    void linkChunkNeighbors(Chunk* chunk);
    bool isChunkLoaded(int cx, int cz);
    void loadChunk(int cx, int cz);
    void unloadChunk(int cx, int cz);

    long long makeKey(int x, int z) const;

private:
    std::unique_ptr<WorldSave> worldSave;

    int renderDistance;
    int renderDistanceSquared;

    int lastPlayerChunkX;
    int lastPlayerChunkZ;

    std::unordered_map<long long, Chunk*> chunks;           // Loaded chunks
    std::mutex chunksMutex;

    std::unordered_set<std::pair<int, int>, pair_hash> chunksBeingGenerated; // Prevent double queue
    std::unordered_set<long long> queuedChunks;           // keys of queued chunks

    std::queue<std::pair<int, int>> generationQueue;
    std::queue<Chunk*> readyChunks;
    std::mutex mutex;
    std::condition_variable queueCV;
    std::thread generationThread;
    bool shouldStop;
};
