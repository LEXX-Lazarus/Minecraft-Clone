#ifndef CHUNK_MANAGER_H
#define CHUNK_MANAGER_H

#include "Chunk.h"
#include "TerrainGenerator.h"

#include <unordered_map>
#include <vector>
#include <utility>
#include <thread>
#include <mutex>
#include <queue>
#include <memory>

// =====================================================
// FAST HASH FOR CHUNK COORDINATES
// =====================================================
struct ChunkCoordHash {
    std::size_t operator()(const std::pair<int, int>& p) const noexcept {
        // 64-bit mix, very fast, low collision
        return (static_cast<std::size_t>(p.first) << 32)
            ^ static_cast<std::size_t>(p.second);
    }
};

class ChunkManager {
public:
    ChunkManager(int renderDistance);
    ~ChunkManager();

    void update(float playerX, float playerZ);
    void render();
    void renderType(BlockType type);

    int getRenderDistance() const { return renderDistance; }

    // NOTE: behavior unchanged (still returns heap Block*)
    // Optimization intentionally avoids altering semantics
    Block* getBlockAt(int worldX, int worldY, int worldZ);

private:
    int renderDistance;

    // =====================================================
    // CHUNK STORAGE (OPTIMIZED)
    // =====================================================
    std::unordered_map<
        std::pair<int, int>,
        Chunk*,
        ChunkCoordHash
    > chunks;

    int lastPlayerChunkX;
    int lastPlayerChunkZ;

    // =====================================================
    // ASYNC GENERATION
    // =====================================================
    std::queue<std::pair<int, int>> generationQueue;
    std::queue<Chunk*> readyChunks;

    std::mutex queueMutex;
    std::mutex chunksMutex;

    std::unique_ptr<std::thread> generationThread;
    bool shouldStopGeneration;

    // =====================================================
    // INTERNAL LOGIC
    // =====================================================
    void generationWorker();
    void processReadyChunks();

    std::pair<int, int> worldToChunkCoords(float worldX, float worldZ);

    void updateChunksAroundPlayer(int playerChunkX, int playerChunkZ);
    void unloadDistantChunks(int playerChunkX, int playerChunkZ);

    bool isChunkLoaded(int chunkX, int chunkZ);

    void loadChunk(int chunkX, int chunkZ);
    void unloadChunk(int chunkX, int chunkZ);

    void linkChunkNeighbors(int chunkX, int chunkZ);
};

#endif