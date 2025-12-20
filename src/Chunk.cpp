#include "Chunk.h"
#include <vector>
#include <iostream>

Chunk::Chunk(int chunkX, int chunkZ)
    : chunkX(chunkX), chunkZ(chunkZ) {
    // Initialize neighbors to null
    for (int i = 0; i < 4; i++) {
        neighbors[i] = nullptr;
    }

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
    // Clean up all meshes
    for (auto& pair : meshes) {
        MeshData& mesh = pair.second;
        if (mesh.VAO) glDeleteVertexArrays(1, &mesh.VAO);
        if (mesh.VBO) glDeleteBuffers(1, &mesh.VBO);
        if (mesh.EBO) glDeleteBuffers(1, &mesh.EBO);
    }
}

Block Chunk::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE_X || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE_Z) {
        return Block(BlockType::AIR);
    }
    return blocks[x][y][z];
}

Block Chunk::getBlockWorld(int worldX, int worldY, int worldZ) const {
    // Convert world coordinates to local chunk coordinates
    int localX = worldX - (chunkX * CHUNK_SIZE_X);
    int localZ = worldZ - (chunkZ * CHUNK_SIZE_Z);

    // If within this chunk, return directly
    if (localX >= 0 && localX < CHUNK_SIZE_X &&
        localZ >= 0 && localZ < CHUNK_SIZE_Z &&
        worldY >= 0 && worldY < CHUNK_SIZE_Y) {
        return blocks[localX][worldY][localZ];
    }

    // Check neighboring chunks
    if (localX < 0 && neighbors[3]) { // West
        return neighbors[3]->getBlockWorld(worldX, worldY, worldZ);
    }
    if (localX >= CHUNK_SIZE_X && neighbors[2]) { // East
        return neighbors[2]->getBlockWorld(worldX, worldY, worldZ);
    }
    if (localZ < 0 && neighbors[1]) { // South
        return neighbors[1]->getBlockWorld(worldX, worldY, worldZ);
    }
    if (localZ >= CHUNK_SIZE_Z && neighbors[0]) { // North
        return neighbors[0]->getBlockWorld(worldX, worldY, worldZ);
    }

    return Block(BlockType::AIR);
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
    if (x < 0 || x >= CHUNK_SIZE_X || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE_Z) {
        return;
    }
    blocks[x][y][z] = Block(type);
}

void Chunk::setNeighbor(int direction, Chunk* neighbor) {
    if (direction >= 0 && direction < 4) {
        neighbors[direction] = neighbor;
    }
}

void Chunk::buildMesh() {
    // Build separate mesh for each block type
    buildMeshForType(BlockType::GRASS);
    buildMeshForType(BlockType::DIRT);
    buildMeshForType(BlockType::STONE);
}

void Chunk::buildMeshForType(BlockType targetType) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int vertexCount = 0;

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                Block block = getBlock(x, y, z);

                if (block.isAir() || block.type != targetType) continue;

                // World position of this block for rendering
                float worldX = chunkX * CHUNK_SIZE_X + x;
                float worldY = y;
                float worldZ = -(chunkZ * CHUNK_SIZE_Z + z);

                // World block coordinates for neighbor checking
                int blockWorldX = chunkX * CHUNK_SIZE_X + x;
                int blockWorldY = y;
                int blockWorldZ = chunkZ * CHUNK_SIZE_Z + z;

                // Top face
                if (getBlockWorld(blockWorldX, blockWorldY + 1, blockWorldZ).isAir()) {
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

                // Bottom face
                if (getBlockWorld(blockWorldX, blockWorldY - 1, blockWorldZ).isAir()) {
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

                // South face
                if (getBlockWorld(blockWorldX, blockWorldY, blockWorldZ - 1).isAir()) {
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

                // North face
                if (getBlockWorld(blockWorldX, blockWorldY, blockWorldZ + 1).isAir()) {
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

                // East face
                if (getBlockWorld(blockWorldX + 1, blockWorldY, blockWorldZ).isAir()) {
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

                // West face
                if (getBlockWorld(blockWorldX - 1, blockWorldY, blockWorldZ).isAir()) {
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

    if (indices.size() > 0) {
        MeshData mesh;
        mesh.indexCount = indices.size();
        setupMesh(mesh, vertices, indices);
        meshes[targetType] = mesh;
    }
}

void Chunk::setupMesh(MeshData& mesh, const std::vector<float>& vertices, const std::vector<unsigned int>& indices) {
    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);
    glGenBuffers(1, &mesh.EBO);

    glBindVertexArray(mesh.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Chunk::render() {
    for (auto& pair : meshes) {
        MeshData& mesh = pair.second;
        if (mesh.indexCount == 0) continue;

        glBindVertexArray(mesh.VAO);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}

void Chunk::renderType(BlockType type) {
    if (meshes.find(type) == meshes.end()) return;

    MeshData& mesh = meshes[type];
    if (mesh.indexCount == 0) return;

    glBindVertexArray(mesh.VAO);
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}