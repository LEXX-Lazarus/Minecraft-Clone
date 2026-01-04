#include "World/Chunk.h"
#include "Rendering/TextureAtlas.h"
#include <vector>
#include <iostream>
#include <cstring>

static bool isTransparentBlock(BlockType type) {
    return type == Blocks::OAK_LEAVES;
}

static bool shouldRenderFace(BlockType currentType, const Block& neighbor) {
    if (neighbor.isAir()) return true;

    bool currentTransparent = isTransparentBlock(currentType);
    bool neighborTransparent = isTransparentBlock(neighbor.type);

    // Opaque block logic (standard Minecraft)
    if (!currentTransparent) {
        return neighbor.isAir() || neighborTransparent;
    }

    // Transparent block logic (leaves)
    // Render face unless neighbor is SAME transparent type
    return !(neighborTransparent && neighbor.type == currentType);
}

Chunk::Chunk(int chunkX, int chunkY, int chunkZ)
    : chunkX(chunkX), chunkY(chunkY), chunkZ(chunkZ) {
    std::memset(neighbors, 0, sizeof(neighbors));

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

    int localX = worldX - (chunkX * CHUNK_SIZE_X);
    int localY = worldY - (chunkY * CHUNK_SIZE_Y);
    int localZ = worldZ - (chunkZ * CHUNK_SIZE_Z);

    if (localX >= 0 && localX < CHUNK_SIZE_X &&
        localY >= 0 && localY < CHUNK_SIZE_Y &&
        localZ >= 0 && localZ < CHUNK_SIZE_Z) {
        return blocks[localX][localY][localZ];
    }

    int targetChunkX = worldX / CHUNK_SIZE_X;
    if (worldX < 0 && worldX % CHUNK_SIZE_X != 0) targetChunkX--;

    int targetChunkY = worldY / CHUNK_SIZE_Y;
    if (worldY < 0 && worldY % CHUNK_SIZE_Y != 0) targetChunkY--;

    int targetChunkZ = worldZ / CHUNK_SIZE_Z;
    if (worldZ < 0 && worldZ % CHUNK_SIZE_Z != 0) targetChunkZ--;

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
    buildMeshForType(Blocks::SAND, atlas);
    buildMeshForType(Blocks::OAK_LOG, atlas);
    buildMeshForType(Blocks::BLOCK_OF_WHITE_LIGHT, atlas);
    buildMeshForType(Blocks::BLOCK_OF_RED_LIGHT, atlas);
    buildMeshForType(Blocks::BLOCK_OF_GREEN_LIGHT, atlas);
    buildMeshForType(Blocks::BLOCK_OF_BLUE_LIGHT, atlas);

    // Build transparent blocks last for proper alpha blending
    buildMeshForType(Blocks::OAK_LEAVES, atlas);
}

void Chunk::buildMeshForType(BlockType targetType, const TextureAtlas* atlas) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    vertices.reserve(32768);
    indices.reserve(49152);

    unsigned int vertexCount = 0;

    const TextureRect topUV = atlas ? atlas->getFaceUVs(targetType, 0) : TextureRect{ 0.25f, 0.666f, 0.5f, 1.0f };
    const TextureRect bottomUV = atlas ? atlas->getFaceUVs(targetType, 1) : TextureRect{ 0.25f, 0.0f, 0.5f, 0.333f };
    const TextureRect sideUV = atlas ? atlas->getFaceUVs(targetType, 2) : TextureRect{ 0.25f, 0.333f, 0.5f, 0.666f };

    const float chunkWorldX = static_cast<float>(chunkX * CHUNK_SIZE_X);
    const float chunkWorldY = static_cast<float>(chunkY * CHUNK_SIZE_Y);
    const float chunkWorldZ = static_cast<float>(chunkZ * CHUNK_SIZE_Z);

    const int blockWorldXBase = chunkX * CHUNK_SIZE_X;
    const int blockWorldYBase = chunkY * CHUNK_SIZE_Y;
    const int blockWorldZBase = chunkZ * CHUNK_SIZE_Z;

    const unsigned int indexPattern[6] = { 0, 1, 2, 2, 3, 0 };

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        const int blockWorldX = blockWorldXBase + x;
        const float worldX = chunkWorldX + x;

        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            const int blockWorldY = blockWorldYBase + y;
            const float worldY = chunkWorldY + y;

            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                const Block& block = blocks[x][y][z];

                if (block.isAir() || block.type != targetType) continue;

                const int blockWorldZ = blockWorldZBase + z;
                const float worldZ = -(chunkWorldZ + z);

                const Block topBlock = getBlockWorld(blockWorldX, blockWorldY + 1, blockWorldZ);
                const Block bottomBlock = getBlockWorld(blockWorldX, blockWorldY - 1, blockWorldZ);
                const Block southBlock = getBlockWorld(blockWorldX, blockWorldY, blockWorldZ - 1);
                const Block northBlock = getBlockWorld(blockWorldX, blockWorldY, blockWorldZ + 1);
                const Block eastBlock = getBlockWorld(blockWorldX + 1, blockWorldY, blockWorldZ);
                const Block westBlock = getBlockWorld(blockWorldX - 1, blockWorldY, blockWorldZ);

                const float xMin = worldX - 0.5f;
                const float xMax = worldX + 0.5f;
                const float yMin = worldY - 0.5f;
                const float yMax = worldY + 0.5f;
                const float zMin = worldZ - 0.5f;
                const float zMax = worldZ + 0.5f;

                if (shouldRenderFace(targetType, topBlock)) {
                    const float v[] = {
                        xMin, yMax, zMax, topUV.uMin, topUV.vMin,
                        xMax, yMax, zMax, topUV.uMax, topUV.vMin,
                        xMax, yMax, zMin, topUV.uMax, topUV.vMax,
                        xMin, yMax, zMin, topUV.uMin, topUV.vMax
                    };
                    vertices.insert(vertices.end(), v, v + 20);
                    for (int i = 0; i < 6; i++) {
                        indices.push_back(vertexCount + indexPattern[i]);
                    }
                    vertexCount += 4;
                }

                if (shouldRenderFace(targetType, bottomBlock)) {
                    const float v[] = {
                        xMin, yMin, zMin, bottomUV.uMin, bottomUV.vMin,
                        xMax, yMin, zMin, bottomUV.uMax, bottomUV.vMin,
                        xMax, yMin, zMax, bottomUV.uMax, bottomUV.vMax,
                        xMin, yMin, zMax, bottomUV.uMin, bottomUV.vMax
                    };
                    vertices.insert(vertices.end(), v, v + 20);
                    for (int i = 0; i < 6; i++) {
                        indices.push_back(vertexCount + indexPattern[i]);
                    }
                    vertexCount += 4;
                }

                if (shouldRenderFace(targetType, southBlock)) {
                    const float v[] = {
                        xMin, yMin, zMax, sideUV.uMin, sideUV.vMin,
                        xMax, yMin, zMax, sideUV.uMax, sideUV.vMin,
                        xMax, yMax, zMax, sideUV.uMax, sideUV.vMax,
                        xMin, yMax, zMax, sideUV.uMin, sideUV.vMax
                    };
                    vertices.insert(vertices.end(), v, v + 20);
                    for (int i = 0; i < 6; i++) {
                        indices.push_back(vertexCount + indexPattern[i]);
                    }
                    vertexCount += 4;
                }

                if (shouldRenderFace(targetType, northBlock)) {
                    const float v[] = {
                        xMax, yMin, zMin, sideUV.uMin, sideUV.vMin,
                        xMin, yMin, zMin, sideUV.uMax, sideUV.vMin,
                        xMin, yMax, zMin, sideUV.uMax, sideUV.vMax,
                        xMax, yMax, zMin, sideUV.uMin, sideUV.vMax
                    };
                    vertices.insert(vertices.end(), v, v + 20);
                    for (int i = 0; i < 6; i++) {
                        indices.push_back(vertexCount + indexPattern[i]);
                    }
                    vertexCount += 4;
                }

                if (shouldRenderFace(targetType, eastBlock)) {
                    const float v[] = {
                        xMax, yMin, zMax, sideUV.uMin, sideUV.vMin,
                        xMax, yMin, zMin, sideUV.uMax, sideUV.vMin,
                        xMax, yMax, zMin, sideUV.uMax, sideUV.vMax,
                        xMax, yMax, zMax, sideUV.uMin, sideUV.vMax
                    };
                    vertices.insert(vertices.end(), v, v + 20);
                    for (int i = 0; i < 6; i++) {
                        indices.push_back(vertexCount + indexPattern[i]);
                    }
                    vertexCount += 4;
                }

                if (shouldRenderFace(targetType, westBlock)) {
                    const float v[] = {
                        xMin, yMin, zMin, sideUV.uMin, sideUV.vMin,
                        xMin, yMin, zMax, sideUV.uMax, sideUV.vMin,
                        xMin, yMax, zMax, sideUV.uMax, sideUV.vMax,
                        xMin, yMax, zMin, sideUV.uMin, sideUV.vMax
                    };
                    vertices.insert(vertices.end(), v, v + 20);
                    for (int i = 0; i < 6; i++) {
                        indices.push_back(vertexCount + indexPattern[i]);
                    }
                    vertexCount += 4;
                }
            }
        }
    }

    if (indices.size() > 0) {
        // Has blocks - create new mesh
        MeshData mesh;
        mesh.indexCount = indices.size();
        setupMesh(mesh, vertices, indices);
        meshes[targetType] = mesh;
    }
    else {
        // No blocks of this type - delete old mesh if it exists
        auto it = meshes.find(targetType);
        if (it != meshes.end()) {
            if (it->second.VAO) glDeleteVertexArrays(1, &it->second.VAO);
            if (it->second.VBO) glDeleteBuffers(1, &it->second.VBO);
            if (it->second.EBO) glDeleteBuffers(1, &it->second.EBO);
            meshes.erase(it);
        }
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

// **CRITICAL FIX**: Render ALL meshes in single pass
void Chunk::render() {
    for (auto& pair : meshes) {
        MeshData& mesh = pair.second;
        if (mesh.indexCount == 0) continue;

        glBindVertexArray(mesh.VAO);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);  // Unbind once at the end
}

void Chunk::renderType(BlockType type) {
    auto it = meshes.find(type);
    if (it == meshes.end()) return;

    MeshData& mesh = it->second;
    if (mesh.indexCount == 0) return;

    glBindVertexArray(mesh.VAO);
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}