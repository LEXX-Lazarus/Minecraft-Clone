#ifndef CHUNK_H
#define CHUNK_H
#include "Block.h"
#include <glad/glad.h>
#include <map>
#include <vector>

// Chunk dimensions
constexpr int CHUNK_SIZE_X = 16;
constexpr int CHUNK_SIZE_Y = 256;
constexpr int CHUNK_SIZE_Z = 16;
constexpr int MAX_HEIGHT = 256;

class Chunk {
public:
    int chunkX, chunkZ;
    Chunk(int chunkX, int chunkZ);
    ~Chunk();

    Block getBlock(int x, int y, int z) const;
    Block getBlockWorld(int worldX, int worldY, int worldZ) const;
    void setBlock(int x, int y, int z, BlockType type);
    void setNeighbor(int direction, Chunk* neighbor);
    Chunk* getNeighbor(int direction) const;

    void buildMesh();
    void render();
    void renderType(BlockType type);

    // Lighting functions
    void calculateSkyLight(unsigned char maxSkyLight = 15);
    void propagateSkyLight();
    void propagateSkyLightCrossChunk();  // NEW: Cross-chunk propagation
    void updateSkyLightLevel(unsigned char newMaxSkyLight);
    unsigned char getSkyLight(int x, int y, int z) const;

private:
    Block blocks[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    Chunk* neighbors[4];  // 0=North, 1=South, 2=East, 3=West

    struct MeshData {
        unsigned int VAO = 0;
        unsigned int VBO = 0;
        unsigned int EBO = 0;
        unsigned int indexCount = 0;
    };

    std::map<BlockType, MeshData> meshes;
    void setupMesh(MeshData& mesh, const std::vector<float>& vertices, const std::vector<unsigned int>& indices);
    void buildMeshForType(BlockType type);
};

#endif