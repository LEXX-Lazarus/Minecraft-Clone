#include "Chunk.h"
#include <vector>
#include <iostream>
#include <queue>
#include <unordered_set>

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
    // Check Y bounds first (no neighbors in Y direction)
    if (worldY < 0 || worldY >= CHUNK_SIZE_Y) {
        return Block(BlockType::AIR);
    }

    // Calculate which chunk this world position belongs to
    int targetChunkX = worldX / CHUNK_SIZE_X;
    if (worldX < 0 && worldX % CHUNK_SIZE_X != 0) targetChunkX--;

    int targetChunkZ = worldZ / CHUNK_SIZE_Z;
    if (worldZ < 0 && worldZ % CHUNK_SIZE_Z != 0) targetChunkZ--;

    // If it's in this chunk, return directly
    if (targetChunkX == chunkX && targetChunkZ == chunkZ) {
        int localX = worldX - (chunkX * CHUNK_SIZE_X);
        int localZ = worldZ - (chunkZ * CHUNK_SIZE_Z);
        return blocks[localX][worldY][localZ];
    }

    // Check which neighbor we need
    int deltaX = targetChunkX - chunkX;
    int deltaZ = targetChunkZ - chunkZ;

    // Direct neighbors only
    if (deltaX == 0 && deltaZ == 1 && neighbors[0]) {  // North
        return neighbors[0]->getBlockWorld(worldX, worldY, worldZ);
    }
    if (deltaX == 0 && deltaZ == -1 && neighbors[1]) {  // South
        return neighbors[1]->getBlockWorld(worldX, worldY, worldZ);
    }
    if (deltaX == 1 && deltaZ == 0 && neighbors[2]) {  // East
        return neighbors[2]->getBlockWorld(worldX, worldY, worldZ);
    }
    if (deltaX == -1 && deltaZ == 0 && neighbors[3]) {  // West
        return neighbors[3]->getBlockWorld(worldX, worldY, worldZ);
    }

    // Neighbor not loaded -> treat as solid so faces are not generated
    return Block(BlockType::STONE);
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

Chunk* Chunk::getNeighbor(int direction) const {
    if (direction < 0 || direction >= 4) return nullptr;
    return neighbors[direction];
}

unsigned char Chunk::getSkyLight(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE_X || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE_Z) {
        return 0;
    }
    return blocks[x][y][z].skyLight;
}

void Chunk::calculateSkyLight(unsigned char maxSkyLight) {
    // Step 1: Initialize ALL blocks to 0
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                blocks[x][y][z].skyLight = 0;
            }
        }
    }

    // Step 2: Direct skylight - scan each column from top
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
                if (blocks[x][y][z].isAir()) {
                    blocks[x][y][z].skyLight = maxSkyLight;
                }
                else {
                    break;
                }
            }
        }
    }

    // Step 3: Propagate within chunk (handles Y-axis perfectly)
    propagateSkyLight();
}

// INTERNAL PROPAGATION: Uses LOCAL coordinates (keeps Y-axis working!)
void Chunk::propagateSkyLight() {
    static bool queued[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                queued[x][y][z] = false;
            }
        }
    }

    std::queue<std::tuple<int, int, int, unsigned char>> lightQueue;

    // CRITICAL FIX: Seed ALL blocks that have ANY light and a dimmer neighbor
    // Not just blocks with max light!
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                // OLD: if (blocks[x][y][z].skyLight != maxLightInChunk || !blocks[x][y][z].isAir())
                // NEW: Seed ANY block with light > 0
                if (blocks[x][y][z].skyLight == 0 || !blocks[x][y][z].isAir())
                    continue;

                unsigned char myLight = blocks[x][y][z].skyLight;
                bool isBoundary = false;

                // Check if ANY neighbor has LESS light
                if (x > 0 && blocks[x - 1][y][z].isAir() && blocks[x - 1][y][z].skyLight < myLight)
                    isBoundary = true;
                if (x < CHUNK_SIZE_X - 1 && blocks[x + 1][y][z].isAir() && blocks[x + 1][y][z].skyLight < myLight)
                    isBoundary = true;

                if (y > 0 && blocks[x][y - 1][z].isAir() && blocks[x][y - 1][z].skyLight < myLight)
                    isBoundary = true;
                if (y < CHUNK_SIZE_Y - 1 && blocks[x][y + 1][z].isAir() && blocks[x][y + 1][z].skyLight < myLight)
                    isBoundary = true;

                if (z > 0 && blocks[x][y][z - 1].isAir() && blocks[x][y][z - 1].skyLight < myLight)
                    isBoundary = true;
                if (z < CHUNK_SIZE_Z - 1 && blocks[x][y][z + 1].isAir() && blocks[x][y][z + 1].skyLight < myLight)
                    isBoundary = true;

                if (isBoundary) {
                    lightQueue.push(std::make_tuple(x, y, z, myLight));
                    queued[x][y][z] = true;
                }
            }
        }
    }

    // Propagate light
    int processed = 0;
    const int MAX_PROCESS = 10000;

    while (!lightQueue.empty() && processed < MAX_PROCESS) {
        processed++;

        auto [x, y, z, currentLight] = lightQueue.front();
        lightQueue.pop();

        currentLight = blocks[x][y][z].skyLight;
        if (currentLight <= 1) continue;

        unsigned char spreadLight = currentLight - 1;

        int dx[] = { 1, -1, 0, 0, 0, 0 };
        int dy[] = { 0, 0, 1, -1, 0, 0 };
        int dz[] = { 0, 0, 0, 0, 1, -1 };

        for (int i = 0; i < 6; i++) {
            int nx = x + dx[i];
            int ny = y + dy[i];
            int nz = z + dz[i];

            // Stay within chunk bounds
            if (nx < 0 || nx >= CHUNK_SIZE_X ||
                ny < 0 || ny >= CHUNK_SIZE_Y ||
                nz < 0 || nz >= CHUNK_SIZE_Z) {
                continue;
            }

            if (queued[nx][ny][nz]) continue;

            Block& neighbor = blocks[nx][ny][nz];
            if (!neighbor.isAir()) continue;

            if (spreadLight > neighbor.skyLight) {
                neighbor.skyLight = spreadLight;

                if (spreadLight > 2) {
                    lightQueue.push(std::make_tuple(nx, ny, nz, spreadLight));
                    queued[nx][ny][nz] = true;
                }
            }
        }
    }
}

// CROSS-CHUNK PROPAGATION: Uses WORLD coordinates (fixes X/Z between chunks!)
void Chunk::propagateSkyLightFloodFill() {
    std::queue<std::tuple<int, int, int>> lightQueue;
    std::unordered_set<long long> queued;

    auto makeWorldKey = [](int x, int y, int z) -> long long {
        return (static_cast<long long>(x + 100000) << 32) |
            (static_cast<long long>(y) << 16) |
            static_cast<long long>(z + 100000);
        };

    // Seed ONLY edge blocks that can propagate to neighbors
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                // Only check edge blocks
                if (x != 0 && x != CHUNK_SIZE_X - 1 && z != 0 && z != CHUNK_SIZE_Z - 1)
                    continue;

                if (!blocks[x][y][z].isAir() || blocks[x][y][z].skyLight == 0)
                    continue;

                int worldX = chunkX * CHUNK_SIZE_X + x;
                int worldY = y;
                int worldZ = chunkZ * CHUNK_SIZE_Z + z;

                unsigned char myLight = blocks[x][y][z].skyLight;

                // Check if we can improve a neighbor's light
                bool canPropagate = false;

                Block neighbor;

                neighbor = getBlockWorld(worldX - 1, worldY, worldZ);
                if (neighbor.isAir() && myLight > neighbor.skyLight + 1) canPropagate = true;

                neighbor = getBlockWorld(worldX + 1, worldY, worldZ);
                if (neighbor.isAir() && myLight > neighbor.skyLight + 1) canPropagate = true;

                neighbor = getBlockWorld(worldX, worldY, worldZ - 1);
                if (neighbor.isAir() && myLight > neighbor.skyLight + 1) canPropagate = true;

                neighbor = getBlockWorld(worldX, worldY, worldZ + 1);
                if (neighbor.isAir() && myLight > neighbor.skyLight + 1) canPropagate = true;

                if (canPropagate) {
                    long long key = makeWorldKey(worldX, worldY, worldZ);
                    lightQueue.push(std::make_tuple(worldX, worldY, worldZ));
                    queued.insert(key);
                }
            }
        }
    }

    // Propagate across chunks (X/Z only, Y is already handled internally)
    int iterations = 0;
    const int MAX_ITERATIONS = 50000;

    while (!lightQueue.empty() && iterations < MAX_ITERATIONS) {
        iterations++;

        auto [worldX, worldY, worldZ] = lightQueue.front();
        lightQueue.pop();

        Block currentBlock = getBlockWorld(worldX, worldY, worldZ);
        if (!currentBlock.isAir()) continue;

        unsigned char currentLight = currentBlock.skyLight;
        if (currentLight <= 1) continue;

        unsigned char spreadLight = currentLight - 1;

        // Only spread in X/Z directions (Y is handled internally)
        int dx[] = { 1, -1, 0, 0 };
        int dz[] = { 0, 0, 1, -1 };

        for (int i = 0; i < 4; i++) {
            int nx = worldX + dx[i];
            int nz = worldZ + dz[i];
            int ny = worldY;

            long long neighborKey = makeWorldKey(nx, ny, nz);
            if (queued.count(neighborKey)) continue;

            Block neighbor = getBlockWorld(nx, ny, nz);
            if (!neighbor.isAir()) continue;

            if (spreadLight > neighbor.skyLight) {
                setBlockWorldLight(nx, ny, nz, spreadLight);

                lightQueue.push(std::make_tuple(nx, ny, nz));
                queued.insert(neighborKey);
            }
        }
    }
}

// Helper to set light across chunks
void Chunk::setBlockWorldLight(int worldX, int worldY, int worldZ, unsigned char lightLevel) {
    if (worldY < 0 || worldY >= CHUNK_SIZE_Y) return;

    int targetChunkX = worldX / CHUNK_SIZE_X;
    if (worldX < 0 && worldX % CHUNK_SIZE_X != 0) targetChunkX--;

    int targetChunkZ = worldZ / CHUNK_SIZE_Z;
    if (worldZ < 0 && worldZ % CHUNK_SIZE_Z != 0) targetChunkZ--;

    if (targetChunkX == chunkX && targetChunkZ == chunkZ) {
        int localX = worldX - (chunkX * CHUNK_SIZE_X);
        int localZ = worldZ - (chunkZ * CHUNK_SIZE_Z);
        blocks[localX][worldY][localZ].skyLight = lightLevel;
        return;
    }

    int deltaX = targetChunkX - chunkX;
    int deltaZ = targetChunkZ - chunkZ;

    if (deltaX == 0 && deltaZ == 1 && neighbors[0]) {
        neighbors[0]->setBlockWorldLight(worldX, worldY, worldZ, lightLevel);
    }
    else if (deltaX == 0 && deltaZ == -1 && neighbors[1]) {
        neighbors[1]->setBlockWorldLight(worldX, worldY, worldZ, lightLevel);
    }
    else if (deltaX == 1 && deltaZ == 0 && neighbors[2]) {
        neighbors[2]->setBlockWorldLight(worldX, worldY, worldZ, lightLevel);
    }
    else if (deltaX == -1 && deltaZ == 0 && neighbors[3]) {
        neighbors[3]->setBlockWorldLight(worldX, worldY, worldZ, lightLevel);
    }
}

void Chunk::updateSkyLightLevel(unsigned char newMaxSkyLight) {
    unsigned char currentMaxLight = 0;
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                if (blocks[x][y][z].skyLight > currentMaxLight) {
                    currentMaxLight = blocks[x][y][z].skyLight;
                }
            }
        }
    }

    if (currentMaxLight == 0) return;

    float ratio = static_cast<float>(newMaxSkyLight) / static_cast<float>(currentMaxLight);

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                if (blocks[x][y][z].skyLight > 0) {
                    float newLightFloat = blocks[x][y][z].skyLight * ratio;
                    unsigned char newLight = static_cast<unsigned char>(std::round(newLightFloat));

                    if (newLight < 1) newLight = 1;
                    if (newLight > 15) newLight = 15;

                    blocks[x][y][z].skyLight = newLight;
                }
            }
        }
    }
}

void Chunk::buildMesh() {
    buildMeshForType(BlockType::GRASS);
    buildMeshForType(BlockType::DIRT);
    buildMeshForType(BlockType::STONE);
    buildMeshForType(BlockType::SAND);
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

                float worldX = chunkX * CHUNK_SIZE_X + x;
                float worldY = y;
                float worldZ = -(chunkZ * CHUNK_SIZE_Z + z);

                int blockWorldX = chunkX * CHUNK_SIZE_X + x;
                int blockWorldY = y;
                int blockWorldZ = chunkZ * CHUNK_SIZE_Z + z;

                // Top face
                if (getBlockWorld(blockWorldX, blockWorldY + 1, blockWorldZ).isAir()) {
                    Block airBlock = getBlockWorld(blockWorldX, blockWorldY + 1, blockWorldZ);
                    float lightLevel = airBlock.skyLight / 15.0f;

                    vertices.insert(vertices.end(), {
                        worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f,   0.25f, 0.666f,  0.0f, 1.0f, 0.0f,  lightLevel,
                        worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f,   0.5f,  0.666f,  0.0f, 1.0f, 0.0f,  lightLevel,
                        worldX + 0.5f, worldY + 0.5f, worldZ - 0.5f,   0.5f,  1.0f,    0.0f, 1.0f, 0.0f,  lightLevel,
                        worldX - 0.5f, worldY + 0.5f, worldZ - 0.5f,   0.25f, 1.0f,    0.0f, 1.0f, 0.0f,  lightLevel
                        });
                    indices.insert(indices.end(), {
                        vertexCount, vertexCount + 1, vertexCount + 2,
                        vertexCount + 2, vertexCount + 3, vertexCount
                        });
                    vertexCount += 4;
                }

                // Bottom face
                if (getBlockWorld(blockWorldX, blockWorldY - 1, blockWorldZ).isAir()) {
                    Block airBlock = getBlockWorld(blockWorldX, blockWorldY - 1, blockWorldZ);
                    float lightLevel = airBlock.skyLight / 15.0f;

                    vertices.insert(vertices.end(), {
                        worldX - 0.5f, worldY - 0.5f, worldZ - 0.5f,   0.25f, 0.0f,    0.0f, -1.0f, 0.0f,  lightLevel,
                        worldX + 0.5f, worldY - 0.5f, worldZ - 0.5f,   0.5f,  0.0f,    0.0f, -1.0f, 0.0f,  lightLevel,
                        worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f,   0.5f,  0.333f,  0.0f, -1.0f, 0.0f,  lightLevel,
                        worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f,   0.25f, 0.333f,  0.0f, -1.0f, 0.0f,  lightLevel
                        });
                    indices.insert(indices.end(), {
                        vertexCount, vertexCount + 1, vertexCount + 2,
                        vertexCount + 2, vertexCount + 3, vertexCount
                        });
                    vertexCount += 4;
                }

                // South face
                if (getBlockWorld(blockWorldX, blockWorldY, blockWorldZ - 1).isAir()) {
                    Block airBlock = getBlockWorld(blockWorldX, blockWorldY, blockWorldZ - 1);
                    float lightLevel = airBlock.skyLight / 15.0f;

                    vertices.insert(vertices.end(), {
                        worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f,   0.25f, 0.333f,  0.0f, 0.0f, -1.0f,  lightLevel,
                        worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f,   0.5f,  0.333f,  0.0f, 0.0f, -1.0f,  lightLevel,
                        worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f,   0.5f,  0.666f,  0.0f, 0.0f, -1.0f,  lightLevel,
                        worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f,   0.25f, 0.666f,  0.0f, 0.0f, -1.0f,  lightLevel
                        });
                    indices.insert(indices.end(), {
                        vertexCount, vertexCount + 1, vertexCount + 2,
                        vertexCount + 2, vertexCount + 3, vertexCount
                        });
                    vertexCount += 4;
                }

                // North face
                if (getBlockWorld(blockWorldX, blockWorldY, blockWorldZ + 1).isAir()) {
                    Block airBlock = getBlockWorld(blockWorldX, blockWorldY, blockWorldZ + 1);
                    float lightLevel = airBlock.skyLight / 15.0f;

                    vertices.insert(vertices.end(), {
                        worldX + 0.5f, worldY - 0.5f, worldZ - 0.5f,   0.75f, 0.333f,  0.0f, 0.0f, 1.0f,  lightLevel,
                        worldX - 0.5f, worldY - 0.5f, worldZ - 0.5f,   1.0f,  0.333f,  0.0f, 0.0f, 1.0f,  lightLevel,
                        worldX - 0.5f, worldY + 0.5f, worldZ - 0.5f,   1.0f,  0.666f,  0.0f, 0.0f, 1.0f,  lightLevel,
                        worldX + 0.5f, worldY + 0.5f, worldZ - 0.5f,   0.75f, 0.666f,  0.0f, 0.0f, 1.0f,  lightLevel
                        });
                    indices.insert(indices.end(), {
                        vertexCount, vertexCount + 1, vertexCount + 2,
                        vertexCount + 2, vertexCount + 3, vertexCount
                        });
                    vertexCount += 4;
                }

                // East face
                if (getBlockWorld(blockWorldX + 1, blockWorldY, blockWorldZ).isAir()) {
                    Block airBlock = getBlockWorld(blockWorldX + 1, blockWorldY, blockWorldZ);
                    float lightLevel = airBlock.skyLight / 15.0f;

                    vertices.insert(vertices.end(), {
                        worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f,   0.5f,  0.333f,  1.0f, 0.0f, 0.0f,  lightLevel,
                        worldX + 0.5f, worldY - 0.5f, worldZ - 0.5f,   0.75f, 0.333f,  1.0f, 0.0f, 0.0f,  lightLevel,
                        worldX + 0.5f, worldY + 0.5f, worldZ - 0.5f,   0.75f, 0.666f,  1.0f, 0.0f, 0.0f,  lightLevel,
                        worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f,   0.5f,  0.666f,  1.0f, 0.0f, 0.0f,  lightLevel
                        });
                    indices.insert(indices.end(), {
                        vertexCount, vertexCount + 1, vertexCount + 2,
                        vertexCount + 2, vertexCount + 3, vertexCount
                        });
                    vertexCount += 4;
                }

                // West face
                if (getBlockWorld(blockWorldX - 1, blockWorldY, blockWorldZ).isAir()) {
                    Block airBlock = getBlockWorld(blockWorldX - 1, blockWorldY, blockWorldZ);
                    float lightLevel = airBlock.skyLight / 15.0f;

                    vertices.insert(vertices.end(), {
                        worldX - 0.5f, worldY - 0.5f, worldZ - 0.5f,   0.0f,  0.333f,  -1.0f, 0.0f, 0.0f,  lightLevel,
                        worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f,   0.25f, 0.333f,  -1.0f, 0.0f, 0.0f,  lightLevel,
                        worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f,   0.25f, 0.666f,  -1.0f, 0.0f, 0.0f,  lightLevel,
                        worldX - 0.5f, worldY + 0.5f, worldZ - 0.5f,   0.0f,  0.666f,  -1.0f, 0.0f, 0.0f,  lightLevel
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
    else {
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

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Normal attribute
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // Light level attribute
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);

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

void Chunk::initializeLightTexture() {
    glGenTextures(1, &lightTexture);
    glBindTexture(GL_TEXTURE_3D, lightTexture);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8UI, CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_3D, 0);
}