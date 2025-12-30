#ifndef CHUNK_H
#define CHUNK_H

#include "Block.h"
#include <glad/glad.h>
#include <map>
#include <vector>

// 3D Chunk System - 16x16x16 blocks
constexpr int CHUNK_SIZE_X = 16;
constexpr int CHUNK_SIZE_Y = 16;  // NOW 16 instead of 256!
constexpr int CHUNK_SIZE_Z = 16;
constexpr int MAX_HEIGHT = 100000;  // World height: 100,000 blocks = 6,250 chunks vertically

class Chunk {
public:
    int chunkX, chunkY, chunkZ;  // ADDED chunkY - now 3D!

    Chunk(int chunkX, int chunkY, int chunkZ);  // CHANGED: 3 parameters
    ~Chunk();

    Block getBlock(int x, int y, int z) const;
    Block getBlockWorld(int worldX, int worldY, int worldZ) const;
    void setBlock(int x, int y, int z, BlockType type);

    void setNeighbor(int direction, Chunk* neighbor);
    Chunk* getNeighbor(int direction) const;  // ADDED getter

    void buildMesh();
    void render();
    void renderType(BlockType type);

    Block blocks[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];

private:
    Chunk* neighbors[6];  // CHANGED: 6 neighbors (added Up/Down)
    // 0=North(+Z), 1=South(-Z), 2=East(+X), 3=West(-X), 4=Up(+Y), 5=Down(-Y)

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