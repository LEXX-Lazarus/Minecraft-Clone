#ifndef CHUNK_H
#define CHUNK_H

#include "Block.h"
#include <glad/glad.h>

class Chunk {
public:
    static const int CHUNK_SIZE_X = 16;
    static const int CHUNK_SIZE_Y = 100;
    static const int CHUNK_SIZE_Z = 16;
    static const int MAX_HEIGHT = 1000; // Max world height limit

    Chunk(int chunkX, int chunkZ);
    ~Chunk();

    void generate();
    void buildMesh();
    void render();

    Block getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockType type);

private:
    int chunkX, chunkZ; // Chunk coordinates in the world
    Block blocks[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];

    unsigned int VAO, VBO, EBO;
    int indexCount;

    void setupMesh();
};

#endif