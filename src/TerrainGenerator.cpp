#include "TerrainGenerator.h"
#include "Noise.h"
#include "Chunk.h"
#include "Block.h"
#include <cmath>
#include <algorithm>

static Noise noise(12345);

// =====================================================
// UTILITY FUNCTIONS
// =====================================================
inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// =====================================================
// RIDGE NOISE (FOR MOUNTAINS)
// =====================================================
inline float ridgeNoise(float n, float sharpness) {
    n = 1.0f - std::abs(n);
    return std::pow(clampf(n, 0.0f, 1.0f), sharpness);
}

// =====================================================
// VERTICAL FALLOFF (HIGH MOUNTAINS)
// =====================================================
inline float verticalFalloff(int y, int maxHeight) {
    if (y < 120) return 1.0f;
    float t = float(y - 120) / float(maxHeight - 120);
    return clampf(1.0f - t * t * t, 0.1f, 1.0f);
}

// =====================================================
// BASE HEIGHT / CONTINENTALNESS
// =====================================================
inline float getBaseHeight(float continental) {
    if (continental < -0.3f) return lerp(65.0f, 90.0f, (continental + 1.0f) / 0.7f);
    if (continental < 0.1f)  return lerp(90.0f, 130.0f, (continental + 0.3f) / 0.4f);
    if (continental < 0.4f)  return lerp(130.0f, 200.0f, (continental - 0.1f) / 0.3f);
    if (continental < 0.7f)  return lerp(200.0f, 280.0f, (continental - 0.4f) / 0.3f);
    return lerp(280.0f, 400.0f, (continental - 0.7f) / 0.3f);
}

// =====================================================
// STEEPNESS
// =====================================================
inline float calculateSteepness(int x, int z, int heightMap[CHUNK_SIZE_X][CHUNK_SIZE_Z]) {
    float h = float(heightMap[x][z]);
    float maxDiff = 0.0f;

    if (x > 0) maxDiff = std::max(maxDiff, std::abs(h - heightMap[x - 1][z]));
    if (x < CHUNK_SIZE_X - 1) maxDiff = std::max(maxDiff, std::abs(h - heightMap[x + 1][z]));
    if (z > 0) maxDiff = std::max(maxDiff, std::abs(h - heightMap[x][z - 1]));
    if (z < CHUNK_SIZE_Z - 1) maxDiff = std::max(maxDiff, std::abs(h - heightMap[x][z + 1]));

    return clampf(maxDiff / 15.0f, 0.0f, 1.0f);
}

// =====================================================
// TERRAIN GENERATION
// =====================================================
void TerrainGenerator::generateFlatTerrain(Chunk& chunk) {

    const int chunkWorldX = chunk.chunkX * CHUNK_SIZE_X;
    const int chunkWorldZ = chunk.chunkZ * CHUNK_SIZE_Z;

    static float densityField[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    static bool isCaveExposed[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];

    int heightMap[CHUNK_SIZE_X][CHUNK_SIZE_Z];
    float biomeMap[CHUNK_SIZE_X][CHUNK_SIZE_Z];
    float steepnessMap[CHUNK_SIZE_X][CHUNK_SIZE_Z];

    // -------------------------------------------------
    // CLEAR CAVE EXPOSURE
    // -------------------------------------------------
    for (int x = 0; x < CHUNK_SIZE_X; x++)
        for (int y = 0; y < CHUNK_SIZE_Y; y++)
            for (int z = 0; z < CHUNK_SIZE_Z; z++)
                isCaveExposed[x][y][z] = false;

    // -------------------------------------------------
    // PASS 1: BIOME + BASE HEIGHT
    // -------------------------------------------------
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        float wx = float(chunkWorldX + x) * 0.0006f;
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            float wz = float(chunkWorldZ + z) * 0.0006f;
            float continental = noise.perlinOctave2D(wx, wz, 4, 0.5f);
            biomeMap[x][z] = continental;
            heightMap[x][z] = int(getBaseHeight(continental));
        }
    }

    // -------------------------------------------------
    // PASS 2: DENSITY FIELD
    // -------------------------------------------------
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        float wx = float(chunkWorldX + x);
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {

            float wz = float(chunkWorldZ + z);
            float biome = biomeMap[x][z];
            float baseH = float(heightMap[x][z]);

            float mountainNoiseScale = 0.0f;
            float mountainAmplitude = 0.0f;
            float ridgeSharpness = 2.0f;

            if (biome < -0.3f) {}
            else if (biome < 0.1f) { mountainNoiseScale = 0.008f; mountainAmplitude = 10.0f; }
            else if (biome < 0.4f) { mountainNoiseScale = 0.012f; mountainAmplitude = 25.0f; }
            else if (biome < 0.7f) { mountainNoiseScale = 0.002f; mountainAmplitude = 100.0f; ridgeSharpness = 2.5f; }
            else { mountainNoiseScale = 0.004f; mountainAmplitude = 200.0f; ridgeSharpness = 3.5f; }

            float ridgeBase = noise.perlinOctave2D(wx * mountainNoiseScale, wz * mountainNoiseScale, 5, 0.5f);
            float ridge = ridgeNoise(ridgeBase, ridgeSharpness);
            float mountainMask = clampf(biome * 1.5f, 0.0f, 1.0f);

            float sharpnessNoise = noise.perlinOctave2D(wx * 0.003f, wz * 0.003f, 3, 0.6f);
            float sharpnessFactor = clampf(sharpnessNoise + 0.3f, 0.0f, 1.0f);

            float mountainHeight = ridge * mountainMask * mountainAmplitude * sharpnessFactor;

            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                float density = (baseH - y) + mountainHeight * verticalFalloff(y, 360);
                density += noise.perlinOctave3D(wx * 0.01f, y * 0.02f, wz * 0.01f, 3, 0.5f) * 8.0f;

                if (y > 200 && mountainMask > 0.3f) {
                    float peak = noise.perlin3D(wx * 0.03f, y * 0.04f, wz * 0.03f);
                    density += ridgeNoise(peak, 3.5f) * 50.0f * verticalFalloff(y, 360);
                }

                densityField[x][y][z] = density;
            }
        }
    }

    // -------------------------------------------------
    // PASS 2.5: STEEPNESS
    // -------------------------------------------------
    for (int x = 0; x < CHUNK_SIZE_X; x++)
        for (int z = 0; z < CHUNK_SIZE_Z; z++)
            steepnessMap[x][z] = calculateSteepness(x, z, heightMap);

    // -------------------------------------------------
    // PASS 3: CAVES (UNCHANGED LOGIC)
    // -------------------------------------------------
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        float wx = float(chunkWorldX + x);
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            float wz = float(chunkWorldZ + z);
            int surfaceY = heightMap[x][z];

            for (int y = 5; y < CHUNK_SIZE_Y - 5; y++) {
                if (densityField[x][y][z] <= 0) continue;

                float depth = float(surfaceY - y);
                float caveMask = clampf(depth / 20.0f, 0.2f, 1.0f);

                float wormA = noise.perlin3D(wx * 0.015f, y * 0.02f, wz * 0.015f);
                float wormB = noise.perlin3D(wx * 0.015f + 500.0f, y * 0.02f + 500.0f, wz * 0.015f + 500.0f);

                if ((wormA * wormA + wormB * wormB) < 0.012f * caveMask) {
                    densityField[x][y][z] = -1.0f;
                    if (depth < 15.0f) isCaveExposed[x][y][z] = true;
                    continue;
                }

                if (y < 60 && depth > 25.0f) {
                    if (noise.perlinOctave3D(wx * 0.03f, y * 0.03f, wz * 0.03f, 3, 0.5f) > 0.65f) {
                        densityField[x][y][z] = -1.0f;
                        continue;
                    }
                }

                if (depth > 10.0f) {
                    float sA = noise.perlin3D(wx * 0.025f, y * 0.03f, wz * 0.025f);
                    float sB = noise.perlin3D(wx * 0.025f + 1000.0f, y * 0.03f, wz * 0.025f + 1000.0f);
                    if ((std::abs(sA) + std::abs(sB)) < 0.15f * caveMask) {
                        densityField[x][y][z] = -1.0f;
                        if (depth < 15.0f) isCaveExposed[x][y][z] = true;
                    }
                }
            }
        }
    }

    // -------------------------------------------------
    // PASS 4: BLOCK PLACEMENT (UNCHANGED)
    // -------------------------------------------------
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {

            int topSolidY = -1;
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
                if (densityField[x][y][z] > 0) { topSolidY = y; break; }
            }

            for (int y = 0; y < CHUNK_SIZE_Y; y++) {

                if (densityField[x][y][z] <= 0) {
                    chunk.setBlock(x, y, z, BlockType::AIR);
                    continue;
                }

                if (y == 0) {
                    chunk.setBlock(x, y, z, BlockType::STONE);
                    continue;
                }

                bool isExposedToSky = (y == topSolidY);
                int solidAbove = 0;
                for (int dy = 1; dy <= 4 && y + dy < CHUNK_SIZE_Y; dy++) {
                    if (densityField[x][y + dy][z] > 0) solidAbove++;
                    else break;
                }

                float steepness = steepnessMap[x][z];
                float heightFactor = clampf((float(y) - 120.0f) / 120.0f, 0.0f, 1.0f);
                float stoneExposure = clampf(steepness * 0.6f + heightFactor * 0.4f +
                    noise.perlin2D((chunkWorldX + x) * 0.05f, (chunkWorldZ + z) * 0.05f) * 0.2f,
                    0.0f, 1.0f);

                if (isExposedToSky) {
                    chunk.setBlock(x, y, z,
                        (y > 240 || stoneExposure > 0.65f) ? BlockType::STONE : BlockType::GRASS);
                }
                else if (solidAbove > 0 && solidAbove <= 10) {
                    chunk.setBlock(x, y, z, BlockType::DIRT);
                }
                else {
                    chunk.setBlock(x, y, z, BlockType::STONE);
                }
            }
        }
    }
}
