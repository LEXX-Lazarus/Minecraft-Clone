#include "TerrainGenerator.h"
#include "PerlinNoise.h"
#include "Chunk.h"
#include "Block.h"
#include <iostream>
#include <cmath>
#include <algorithm> // for std::max/std::min

// Same seed = same world
static PerlinNoise perlin(12345);

// Helper 3D octave noise function
static float octaveNoise3D(float x, float y, float z, int octaves, float persistence) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; i++) {
        total += perlin.noise(x * frequency, y * frequency, z * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }
    return total / maxValue;
}

void TerrainGenerator::generateFlatTerrain(Chunk& chunk) {
    int chunkWorldX = chunk.chunkX * CHUNK_SIZE_X;
    int chunkWorldZ = chunk.chunkZ * CHUNK_SIZE_Z;

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {

            float worldX = static_cast<float>(chunkWorldX + x);
            float worldZ = static_cast<float>(chunkWorldZ + z);

            // =====================
            // HEIGHT MAP: hills & mountains
            // =====================
            float hillNoise = octaveNoise3D(worldX * 0.005f, 0.0f, worldZ * 0.005f, 4, 0.5f);
            float mountainNoise = octaveNoise3D(worldX * 0.001f, 0.0f, worldZ * 0.001f, 6, 0.4f);

            int baseHeight = 12;
            int hillHeight = static_cast<int>((hillNoise + 1.0f) * 8.0f);       // small hills
            int mountainHeight = static_cast<int>((mountainNoise + 1.0f) * 40.0f); // tall peaks

            int surfaceHeight = baseHeight + hillHeight + mountainHeight;
            surfaceHeight = std::max(1, std::min(surfaceHeight, CHUNK_SIZE_Y - 1));

            // =====================
            // TERRAIN BLOCK LAYERING
            // =====================
            for (int y = 0; y <= surfaceHeight; y++) {
                if (y == surfaceHeight)
                    chunk.setBlock(x, y, z, BlockType::GRASS);
                else if (y >= surfaceHeight - 4)
                    chunk.setBlock(x, y, z, BlockType::DIRT);
                else
                    chunk.setBlock(x, y, z, BlockType::STONE);
            }

            // =====================
            // CAVE SYSTEM
            // =====================
            for (int y = 2; y <= surfaceHeight; y++) {
                float wx = worldX * 0.08f;
                float wy = static_cast<float>(y) * 0.08f;
                float wz = worldZ * 0.08f;

                // Windy tunnels
                float tunnelNoise = octaveNoise3D(wx + 200.0f, wy, wz + 200.0f, 3, 0.6f);
                // Large caverns
                float cavernNoise = octaveNoise3D(wx * 0.4f + 500.0f, wy * 0.5f, wz * 0.4f + 500.0f, 2, 0.5f);
                // Tiny noisy gaps
                float microNoise = octaveNoise3D(wx * 3.0f, wy * 1.5f, wz * 3.0f, 1, 0.5f);

                // Depth bias: more caves underground
                float depth = static_cast<float>(y) / static_cast<float>(surfaceHeight);
                float depthBias = 1.0f - depth;

                bool carveTunnel = tunnelNoise > (0.6f - depthBias * 0.25f);
                bool carveCavern = cavernNoise > 0.75f;
                bool carveMicro = microNoise > 0.85f;

                if (carveTunnel || carveCavern || carveMicro)
                    chunk.setBlock(x, y, z, BlockType::AIR);
            }
        }
    }

    std::cout << "Terrain with hills, mountains, and varied caves generated for chunk ("
        << chunk.chunkX << ", " << chunk.chunkZ << ")\n";
}