#include "TerrainGenerator.h"
#include "Noise.h"
#include "Chunk.h"
#include "Block.h"
#include <iostream>
#include <cmath>
#include <algorithm>

// Same seed = same world
static Noise noise(12345);

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
            float hillNoise = noise.perlinOctave2D(worldX * 0.005f, worldZ * 0.005f, 3, 0.5f);  // 4 -> 3
            float mountainNoise = noise.perlinOctave2D(worldX * 0.001f, worldZ * 0.001f, 4, 0.4f);  // 6 -> 4

            int baseHeight = 12;
            int hillHeight = static_cast<int>((hillNoise + 1.0f) * 8.0f);
            int mountainHeight = static_cast<int>((mountainNoise + 1.0f) * 40.0f);
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
            // CAVE SYSTEM - Using Simplex & Worley
            // =====================
            for (int y = 2; y <= surfaceHeight; y++) {
                float wx = worldX * 0.08f;
                float wy = static_cast<float>(y) * 0.08f;
                float wz = worldZ * 0.08f;

                // RAVINES - Large dramatic surface openings (2D noise, ignores Y)
                float ravineNoise = noise.perlinOctave2D(worldX * 0.02f, worldZ * 0.02f, 2, 0.5f);
                float ravineDepth = noise.perlinOctave2D(worldX * 0.015f + 500.0f, worldZ * 0.015f + 500.0f, 1, 0.5f);

                // Ravine carves from surface down to a certain depth
                int ravineBottomY = surfaceHeight - static_cast<int>((ravineDepth + 1.0f) * 20.0f);  // 0-40 blocks deep
                bool carveRavine = (ravineNoise > 0.6f) && (y > ravineBottomY) && (y < surfaceHeight);

                // SPAGHETTI CAVES (winding tunnels)
                float spaghetti1 = noise.simplexOctave3D(wx, wy, wz, 2, 0.6f);
                float spaghetti2 = noise.simplexOctave3D(wx + 200.0f, wy, wz + 200.0f, 2, 0.6f);
                float spaghettiCombined = std::sqrt(spaghetti1 * spaghetti1 + spaghetti2 * spaghetti2);

                // Depth bias
                float depth = static_cast<float>(y) / static_cast<float>(surfaceHeight);
                float depthBias = 1.0f - depth;

                // Surface distance
                float surfaceDistance = static_cast<float>(surfaceHeight - y);
                float surfaceProtection = std::min(1.0f, surfaceDistance / 8.0f);

                // SPAGHETTI: Can create natural entrances
                bool carveSpaghetti = spaghettiCombined < (0.28f - depthBias * 0.08f) * (0.4f + surfaceProtection * 0.6f);

                // CHEESE CAVES - only deep
                bool carveCheese = false;
                if (y < surfaceHeight - 15) {
                    float cheeseCave = noise.worley3D(wx * 0.4f, wy * 0.5f, wz * 0.4f);
                    carveCheese = (cheeseCave < 0.4f);
                }

                // MICRO CAVES - only below surface
                bool carveMicro = false;
                if (y < surfaceHeight - 3) {
                    float microNoise = noise.simplex3D(wx * 2.0f, wy * 1.0f, wz * 2.0f);
                    carveMicro = microNoise > (0.9f - depthBias * 0.05f);
                }

                if (carveRavine || carveSpaghetti || carveCheese || carveMicro) {
                    chunk.setBlock(x, y, z, BlockType::AIR);
                }
            }
        }
    }

    std::cout << "Terrain with hills, mountains, and Minecraft-style caves generated for chunk ("
        << chunk.chunkX << ", " << chunk.chunkZ << ")\n";
}