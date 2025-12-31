#ifndef CHUNKMANAGER_H
#define CHUNKMANAGER_H

#include "Chunk.h"
#include "WorldSave.h"
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <tuple>
#include <memory>

class TextureAtlas;

// Hash function for tuple
namespace std {
    template<>
    struct hash<std::tuple<int, int, int>> {
        size_t operator()(const std::tuple<int, int, int>& t) const {
            auto h1 = std::hash<int>{}(std::get<0>(t));
            auto h2 = std::hash<int>{}(std::get<1>(t));
            auto h3 = std::hash<int>{}(std::get<2>(t));
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

struct ChunkDistanceEntry {
    int x, y, z;
    int distSq;
};

class ChunkManager {
public:
    ChunkManager(int renderDistance, const std::string& worldName = "world1");
    ~ChunkManager();

    void update(float playerX, float playerY, float playerZ);
    void render();
    void renderType(BlockType type);

    void setTextureAtlas(TextureAtlas* atlas);

    Block* getBlockAt(int worldX, int worldY, int worldZ);
    Block getBlockData(int worldX, int worldY, int worldZ);
    bool setBlockAt(int worldX, int worldY, int worldZ, BlockType type);
    void rebuildChunkMeshAt(int worldX, int worldY, int worldZ);

    bool isChunkLoaded(int cx, int cy, int cz);
    void loadChunk(int cx, int cy, int cz);
    void unloadChunk(int cx, int cy, int cz);

    std::vector<Chunk*> getLoadedChunks();
    std::tuple<int, int, int> worldToChunkCoords(float x, float y, float z);

    void setVerticalRenderDistance(int distance);
    int getVerticalRenderDistance() const;

private:
    int renderDistance;
    int renderDistanceSquared;
    int verticalRenderDistance;
    int lastPlayerChunkX, lastPlayerChunkY, lastPlayerChunkZ;

    std::unordered_map<long long, Chunk*> chunks;
    std::mutex chunksMutex;

    std::queue<Chunk*> readyChunks;
    std::queue<std::tuple<int, int, int>> generationQueue;
    std::unordered_set<long long> queuedChunks;
    std::unordered_set<std::tuple<int, int, int>> chunksBeingGenerated;

    std::vector<std::thread> workerThreads;
    std::mutex mutex;
    std::condition_variable queueCV;
    bool shouldStop;

    TextureAtlas* textureAtlas;

    std::unique_ptr<WorldSave> worldSave;

    long long makeKey(int x, int y, int z) const;
    void generationWorker();
    void updateDesiredChunks(int pcx, int pcy, int pcz);
    void processReadyChunks();
    void unloadDistantChunks(int pcx, int pcy, int pcz);
    void linkChunkNeighbors(Chunk* chunk);
    void linkChunkNeighborsUnsafe(Chunk* chunk);
};

#endif