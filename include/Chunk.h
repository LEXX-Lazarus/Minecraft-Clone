#ifndef CHUNK_H
#define CHUNK_H

#include "Block.h"
#include <glad/glad.h>
#include <map>
#include <vector>

// Chunk dimensions
constexpr int CHUNK_SIZE_X = 16;
constexpr int CHUNK_SIZE_Y = 100;
constexpr int CHUNK_SIZE_Z = 16;
constexpr int MAX_HEIGHT = 1000;

class Chunk {
public:
    Chunk(int chunkX, int chunkZ);
    ~Chunk();

    Block getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockType type);
    void buildMesh();
    void render();
    void renderType(BlockType type);  // NEW: render specific block type

private:
    int chunkX, chunkZ;
    Block blocks[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];

    struct MeshData {
        unsigned int VAO = 0;
        unsigned int VBO = 0;
        unsigned int EBO = 0;
        unsigned int indexCount = 0;
    };

    std::map<BlockType, MeshData> meshes;  // NEW: separate mesh per block type

    void setupMesh(MeshData& mesh, const std::vector<float>& vertices, const std::vector<unsigned int>& indices);
    void buildMeshForType(BlockType type);  // NEW: build mesh for one type
};

#endif