#include "TerrainGenerator.h"
#include "PerlinNoise.h"
#include <iostream>
#include <cmath>

// Static perlin noise instance (same seed = same world)
static PerlinNoise perlin(12345);  // Change seed for different worlds

void TerrainGenerator::generateFlatTerrain(Chunk& chunk) {
    // Get chunk world position
    int chunkWorldX = chunk.chunkX * CHUNK_SIZE_X;
    int chunkWorldZ = chunk.chunkZ * CHUNK_SIZE_Z;

    // Generate natural terrain
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            // World coordinates for this block
            float worldX = chunkWorldX + x;
            float worldZ = chunkWorldZ + z;

            // Get noise value (scale down coordinates for larger features)
            float noiseValue = perlin.octaveNoise(
                worldX * 0.01f,  // Frequency (smaller = larger hills)
                worldZ * 0.01f,
                4,                // Octaves (more = more detail)
                0.5f              // Persistence (detail strength)
            );

            // Convert noise (-1 to 1) to height (0 to 30)
            int baseHeight = 10;  // Minimum height
            int heightVariation = 15;  // Maximum additional height
            int height = baseHeight + static_cast<int>((noiseValue + 1.0f) * 0.5f * heightVariation);

            // Clamp height
            if (height < 0) height = 0;
            if (height >= CHUNK_SIZE_Y) height = CHUNK_SIZE_Y - 1;

            // Place grass on top
            chunk.setBlock(x, height, z, BlockType::GRASS);

            // Place dirt below (3-5 layers)
            int dirtDepth = 3 + static_cast<int>((noiseValue + 1.0f) * 1.0f);
            for (int y = height - 1; y >= height - dirtDepth && y >= 0; y--) {
                chunk.setBlock(x, y, z, BlockType::DIRT);
            }

            // Place stone below dirt (down to y=0)
            for (int y = height - dirtDepth - 1; y >= 0; y--) {
                chunk.setBlock(x, y, z, BlockType::STONE);
            }
        }
    }

    std::cout << "Natural terrain generated for chunk ("
        << chunk.chunkX << ", " << chunk.chunkZ << ")" << std::endl;
}