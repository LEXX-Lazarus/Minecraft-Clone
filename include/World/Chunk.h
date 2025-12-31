#ifndef CHUNK_H
#define CHUNK_H

#include "Block.h"
#include <glad/glad.h>
#include <map>
#include <vector>

class TextureAtlas;

constexpr int CHUNK_SIZE_X = 16;
constexpr int CHUNK_SIZE_Y = 16;
constexpr int CHUNK_SIZE_Z = 16;
constexpr int MAX_HEIGHT = 100000;

class Chunk {
public:
    int chunkX, chunkY, chunkZ;

    Chunk(int chunkX, int chunkY, int chunkZ);
    ~Chunk();

    Block getBlock(int x, int y, int z) const;
    Block getBlockWorld(int worldX, int worldY, int worldZ) const;
    void setBlock(int x, int y, int z, BlockType type);

    void setNeighbor(int direction, Chunk* neighbor);
    Chunk* getNeighbor(int direction) const;

    void buildMesh(const TextureAtlas* atlas = nullptr);
    void render();
    void renderType(BlockType type);

    Block blocks[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];

private:
    Chunk* neighbors[6];

    struct MeshData {
        unsigned int VAO = 0;
        unsigned int VBO = 0;
        unsigned int EBO = 0;
        unsigned int indexCount = 0;
    };

    std::map<BlockType, MeshData> meshes;

    void setupMesh(MeshData& mesh, const std::vector<float>& vertices, const std::vector<unsigned int>& indices);
    void buildMeshForType(BlockType type, const TextureAtlas* atlas);
};

#endif