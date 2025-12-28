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

    // Block access
    Block getBlock(int x, int y, int z) const;
    Block getBlockWorld(int worldX, int worldY, int worldZ) const;
    void setBlock(int x, int y, int z, BlockType type);

    // Neighbor management
    void setNeighbor(int direction, Chunk* neighbor);
    Chunk* getNeighbor(int direction) const;

    // Rendering
    void buildMesh();
    void render();
    void renderType(BlockType type);

    // Lighting functions
    void calculateSkyLight(unsigned char maxSkyLight = 15);
    void propagateSkyLight();  // Internal propagation (LOCAL coords, handles Y-axis)
    void propagateSkyLightFloodFill();  // Cross-chunk propagation (WORLD coords, X/Z only)
    void setBlockWorldLight(int worldX, int worldY, int worldZ, unsigned char lightLevel);
    void updateSkyLightLevel(unsigned char newMaxSkyLight);
    unsigned char getSkyLight(int x, int y, int z) const;

    // GPU 3D texture for skylight (optional, may not be used)
    GLuint lightTexture = 0;
    void initializeLightTexture();

    // Public access to blocks for direct neighbor updates
    Block blocks[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];

private:
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