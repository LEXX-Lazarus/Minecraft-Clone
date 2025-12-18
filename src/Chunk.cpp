#include "Chunk.h"
#include <vector>
#include <iostream>

Chunk::Chunk(int chunkX, int chunkZ)
    : chunkX(chunkX), chunkZ(chunkZ), VAO(0), VBO(0), EBO(0), indexCount(0) {
    // Initialize all blocks as air
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                blocks[x][y][z] = Block(BlockType::AIR);
            }
        }
    }
}

Chunk::~Chunk() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (EBO) glDeleteBuffers(1, &EBO);
}

Block Chunk::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE_X || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE_Z) {
        return Block(BlockType::AIR);
    }
    return blocks[x][y][z];
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
    if (x < 0 || x >= CHUNK_SIZE_X || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE_Z) {
        return;
    }
    blocks[x][y][z] = Block(type);
}

void Chunk::generate() {
    // Generate flat terrain - all grass blocks at y=0
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            setBlock(x, 0, z, BlockType::GRASS);
        }
    }

    std::cout << "Chunk (" << chunkX << ", " << chunkZ << ") generated" << std::endl;
}

void Chunk::buildMesh() {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    unsigned int vertexCount = 0;

    // Iterate through all blocks in the chunk
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                Block block = getBlock(x, y, z);

                if (block.isAir()) continue;

                // World position of this block
                float worldX = chunkX * CHUNK_SIZE_X + x;
                float worldY = y;
                float worldZ = -(chunkZ * CHUNK_SIZE_Z + z);

                // Top face (y+1)
                if (getBlock(x, y + 1, z).isAir()) {
                    vertices.insert(vertices.end(), {
                        worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f,   0.25f, 0.666f,
                        worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f,   0.5f,  0.666f,
                        worldX + 0.5f, worldY + 0.5f, worldZ - 0.5f,   0.5f,  1.0f,
                        worldX - 0.5f, worldY + 0.5f, worldZ - 0.5f,   0.25f, 1.0f
                        });
                    indices.insert(indices.end(), {
                        vertexCount, vertexCount + 1, vertexCount + 2,
                        vertexCount + 2, vertexCount + 3, vertexCount
                        });
                    vertexCount += 4;
                }

                // Bottom face (y-1)
                if (getBlock(x, y - 1, z).isAir()) {
                    vertices.insert(vertices.end(), {
                        worldX - 0.5f, worldY - 0.5f, worldZ - 0.5f,   0.25f, 0.0f,
                        worldX + 0.5f, worldY - 0.5f, worldZ - 0.5f,   0.5f,  0.0f,
                        worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f,   0.5f,  0.333f,
                        worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f,   0.25f, 0.333f
                        });
                    indices.insert(indices.end(), {
                        vertexCount, vertexCount + 1, vertexCount + 2,
                        vertexCount + 2, vertexCount + 3, vertexCount
                        });
                    vertexCount += 4;
                }

                // South face (faces +Z direction) - CHECK z-1 since Z is negated
                if (getBlock(x, y, z - 1).isAir()) {
                    vertices.insert(vertices.end(), {
                        worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f,   0.25f, 0.333f,
                        worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f,   0.5f,  0.333f,
                        worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f,   0.5f,  0.666f,
                        worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f,   0.25f, 0.666f
                        });
                    indices.insert(indices.end(), {
                        vertexCount, vertexCount + 1, vertexCount + 2,
                        vertexCount + 2, vertexCount + 3, vertexCount
                        });
                    vertexCount += 4;
                }

                // North face (faces -Z direction) - CHECK z+1 since Z is negated
                if (getBlock(x, y, z + 1).isAir()) {
                    vertices.insert(vertices.end(), {
                        worldX + 0.5f, worldY - 0.5f, worldZ - 0.5f,   0.75f, 0.333f,
                        worldX - 0.5f, worldY - 0.5f, worldZ - 0.5f,   1.0f,  0.333f,
                        worldX - 0.5f, worldY + 0.5f, worldZ - 0.5f,   1.0f,  0.666f,
                        worldX + 0.5f, worldY + 0.5f, worldZ - 0.5f,   0.75f, 0.666f
                        });
                    indices.insert(indices.end(), {
                        vertexCount, vertexCount + 1, vertexCount + 2,
                        vertexCount + 2, vertexCount + 3, vertexCount
                        });
                    vertexCount += 4;
                }

                // East face (x+1)
                if (getBlock(x + 1, y, z).isAir()) {
                    vertices.insert(vertices.end(), {
                        worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f,   0.5f,  0.333f,
                        worldX + 0.5f, worldY - 0.5f, worldZ - 0.5f,   0.75f, 0.333f,
                        worldX + 0.5f, worldY + 0.5f, worldZ - 0.5f,   0.75f, 0.666f,
                        worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f,   0.5f,  0.666f
                        });
                    indices.insert(indices.end(), {
                        vertexCount, vertexCount + 1, vertexCount + 2,
                        vertexCount + 2, vertexCount + 3, vertexCount
                        });
                    vertexCount += 4;
                }

                // West face (x-1)
                if (getBlock(x - 1, y, z).isAir()) {
                    vertices.insert(vertices.end(), {
                        worldX - 0.5f, worldY - 0.5f, worldZ - 0.5f,   0.0f,  0.333f,
                        worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f,   0.25f, 0.333f,
                        worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f,   0.25f, 0.666f,
                        worldX - 0.5f, worldY + 0.5f, worldZ - 0.5f,   0.0f,  0.666f
                        });
                    indices.insert(indices.end(), {
                        vertexCount, vertexCount + 1, vertexCount + 2,
                        vertexCount + 2, vertexCount + 3, vertexCount
                        });
                    vertexCount += 4;
                }
            }
        }
    }

    indexCount = indices.size();

    if (indexCount == 0) {
        std::cout << "Chunk (" << chunkX << ", " << chunkZ << ") has no visible faces" << std::endl;
        return;
    }

    setupMesh();

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    std::cout << "Chunk (" << chunkX << ", " << chunkZ << ") mesh built: "
        << vertexCount << " vertices, " << indexCount << " indices" << std::endl;
}

void Chunk::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Chunk::render() {
    if (indexCount == 0) return;

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}