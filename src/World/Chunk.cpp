#include "World/Chunk.h"
#include "Rendering/TextureAtlas.h"
#include <vector>
#include <iostream>

Chunk::Chunk(int chunkX, int chunkY, int chunkZ)
    : chunkX(chunkX), chunkY(chunkY), chunkZ(chunkZ) {
    for (int i = 0; i < 6; i++) {
        neighbors[i] = nullptr;
    }

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                blocks[x][y][z] = Block(Blocks::AIR);
            }
        }
    }
}

Chunk::~Chunk() {
    for (auto& pair : meshes) {
        MeshData& mesh = pair.second;
        if (mesh.VAO) glDeleteVertexArrays(1, &mesh.VAO);
        if (mesh.VBO) glDeleteBuffers(1, &mesh.VBO);
        if (mesh.EBO) glDeleteBuffers(1, &mesh.EBO);
    }
}

Block Chunk::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE_X || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE_Z) {
        return Block(Blocks::AIR);
    }
    return blocks[x][y][z];
}

Block Chunk::getBlockWorld(int worldX, int worldY, int worldZ) const {
    if (worldY < 0 || worldY >= MAX_HEIGHT) {
        return Block(Blocks::AIR);
    }

    int targetChunkX = worldX / CHUNK_SIZE_X;
    if (worldX < 0 && worldX % CHUNK_SIZE_X != 0) targetChunkX--;

    int targetChunkY = worldY / CHUNK_SIZE_Y;
    if (worldY < 0 && worldY % CHUNK_SIZE_Y != 0) targetChunkY--;

    int targetChunkZ = worldZ / CHUNK_SIZE_Z;
    if (worldZ < 0 && worldZ % CHUNK_SIZE_Z != 0) targetChunkZ--;

    if (targetChunkX == chunkX && targetChunkY == chunkY && targetChunkZ == chunkZ) {
        int localX = worldX - (chunkX * CHUNK_SIZE_X);
        int localY = worldY - (chunkY * CHUNK_SIZE_Y);
        int localZ = worldZ - (chunkZ * CHUNK_SIZE_Z);

        if (localX < 0 || localX >= CHUNK_SIZE_X ||
            localY < 0 || localY >= CHUNK_SIZE_Y ||
            localZ < 0 || localZ >= CHUNK_SIZE_Z) {
            return Block(Blocks::AIR);
        }

        return blocks[localX][localY][localZ];
    }

    int deltaX = targetChunkX - chunkX;
    int deltaY = targetChunkY - chunkY;
    int deltaZ = targetChunkZ - chunkZ;

    if (deltaY == 0 && deltaX == 0 && deltaZ == 1 && neighbors[0]) {
        return neighbors[0]->getBlockWorld(worldX, worldY, worldZ);
    }
    if (deltaY == 0 && deltaX == 0 && deltaZ == -1 && neighbors[1]) {
        return neighbors[1]->getBlockWorld(worldX, worldY, worldZ);
    }
    if (deltaY == 0 && deltaX == 1 && deltaZ == 0 && neighbors[2]) {
        return neighbors[2]->getBlockWorld(worldX, worldY, worldZ);
    }
    if (deltaY == 0 && deltaX == -1 && deltaZ == 0 && neighbors[3]) {
        return neighbors[3]->getBlockWorld(worldX, worldY, worldZ);
    }
    if (deltaX == 0 && deltaZ == 0 && deltaY == 1 && neighbors[4]) {
        return neighbors[4]->getBlockWorld(worldX, worldY, worldZ);
    }
    if (deltaX == 0 && deltaZ == 0 && deltaY == -1 && neighbors[5]) {
        return neighbors[5]->getBlockWorld(worldX, worldY, worldZ);
    }

    return Block(Blocks::STONE);
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
    if (x < 0 || x >= CHUNK_SIZE_X || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE_Z) {
        return;
    }
    blocks[x][y][z] = Block(type);
}

void Chunk::setNeighbor(int direction, Chunk* neighbor) {
    if (direction >= 0 && direction < 6) {
        neighbors[direction] = neighbor;
    }
}

Chunk* Chunk::getNeighbor(int direction) const {
    if (direction < 0 || direction >= 6) return nullptr;
    return neighbors[direction];
}

void Chunk::buildMesh(const TextureAtlas* atlas) {
    buildMeshForType(Blocks::GRASS, atlas);
    buildMeshForType(Blocks::DIRT, atlas);
    buildMeshForType(Blocks::STONE, atlas);
}

void Chunk::buildMeshForType(BlockType targetType, const TextureAtlas* atlas) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // OPTIMIZATION: Pre-allocate memory to avoid reallocations
    // Worst case: all blocks visible with all 6 faces = 4096 * 6 * 4 vertices
    vertices.reserve(4096 * 6 * 5);  // 5 floats per vertex (pos + UV)
    indices.reserve(4096 * 6 * 6);    // 6 indices per face

    unsigned int vertexCount = 0;

    // OPTIMIZATION: Cache UV coordinates outside loop (they're the same for all blocks of this type)
    TextureRect topUV = atlas ? atlas->getFaceUVs(targetType, 0) : TextureRect{ 0.25f, 0.666f, 0.5f, 1.0f };
    TextureRect bottomUV = atlas ? atlas->getFaceUVs(targetType, 1) : TextureRect{ 0.25f, 0.0f, 0.5f, 0.333f };
    TextureRect sideUV = atlas ? atlas->getFaceUVs(targetType, 2) : TextureRect{ 0.25f, 0.333f, 0.5f, 0.666f };

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                Block block = getBlock(x, y, z);

                if (block.isAir() || block.type != targetType) continue;

                float worldX = chunkX * CHUNK_SIZE_X + x;
                float worldY = chunkY * CHUNK_SIZE_Y + y;
                float worldZ = -(chunkZ * CHUNK_SIZE_Z + z);

                int blockWorldX = chunkX * CHUNK_SIZE_X + x;
                int blockWorldY = chunkY * CHUNK_SIZE_Y + y;
                int blockWorldZ = chunkZ * CHUNK_SIZE_Z + z;

                // Top face
                if (getBlockWorld(blockWorldX, blockWorldY + 1, blockWorldZ).isAir()) {
                    vertices.insert(vertices.end(), {
                        worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f,   topUV.uMin, topUV.vMin,
                        worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f,   topUV.uMax, topUV.vMin,
                        worldX + 0.5f, worldY + 0.5f, worldZ - 0.5f,   topUV.uMax, topUV.vMax,
                        worldX - 0.5f, worldY + 0.5f, worldZ - 0.5f,   topUV.uMin, topUV.vMax
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
                        worldX - 0.5f, worldY - 0.5f, worldZ - 0.5f,   bottomUV.uMin, bottomUV.vMin,
                        worldX + 0.5f, worldY - 0.5f, worldZ - 0.5f,   bottomUV.uMax, bottomUV.vMin,
                        worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f,   bottomUV.uMax, bottomUV.vMax,
                        worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f,   bottomUV.uMin, bottomUV.vMax
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
                        worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f,   sideUV.uMin, sideUV.vMin,
                        worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f,   sideUV.uMax, sideUV.vMin,
                        worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f,   sideUV.uMax, sideUV.vMax,
                        worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f,   sideUV.uMin, sideUV.vMax
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
                        worldX + 0.5f, worldY - 0.5f, worldZ - 0.5f,   sideUV.uMin, sideUV.vMin,
                        worldX - 0.5f, worldY - 0.5f, worldZ - 0.5f,   sideUV.uMax, sideUV.vMin,
                        worldX - 0.5f, worldY + 0.5f, worldZ - 0.5f,   sideUV.uMax, sideUV.vMax,
                        worldX + 0.5f, worldY + 0.5f, worldZ - 0.5f,   sideUV.uMin, sideUV.vMax
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
                        worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f,   sideUV.uMin, sideUV.vMin,
                        worldX + 0.5f, worldY - 0.5f, worldZ - 0.5f,   sideUV.uMax, sideUV.vMin,
                        worldX + 0.5f, worldY + 0.5f, worldZ - 0.5f,   sideUV.uMax, sideUV.vMax,
                        worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f,   sideUV.uMin, sideUV.vMax
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
                        worldX - 0.5f, worldY - 0.5f, worldZ - 0.5f,   sideUV.uMin, sideUV.vMin,
                        worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f,   sideUV.uMax, sideUV.vMin,
                        worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f,   sideUV.uMax, sideUV.vMax,
                        worldX - 0.5f, worldY + 0.5f, worldZ - 0.5f,   sideUV.uMin, sideUV.vMax
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