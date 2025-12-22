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
float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

// =====================================================
// RIDGE NOISE (FOR MOUNTAINS)
// =====================================================
float ridgeNoise(float n, float sharpness) {
    n = 1.0f - std::abs(n);
    return std::pow(clampf(n, 0.0f, 1.0f), sharpness);
}

// =====================================================
// VERTICAL FALLOFF (HIGH MOUNTAINS)
// =====================================================
float verticalFalloff(int y, int maxHeight) {
    if (y < 120) return 1.0f;
    float t = float(y - 120) / float(maxHeight - 120);
    // Stronger falloff to prevent floating islands
    return clampf(1.0f - t * t * t, 0.1f, 1.0f);
}

// =====================================================
// BASE HEIGHT / CONTINENTALNESS
// =====================================================
float getBaseHeight(float continental) {
    if (continental < -0.3f) return lerp(65.0f, 90.0f, (continental + 1.0f) / 0.7f);   // plains
    if (continental < 0.1f)  return lerp(90.0f, 130.0f, (continental + 0.3f) / 0.4f);  // hills
    if (continental < 0.4f)  return lerp(130.0f, 200.0f, (continental - 0.1f) / 0.3f); // foothills
    if (continental < 0.7f)  return lerp(200.0f, 280.0f, (continental - 0.4f) / 0.3f); // mountain range
    return lerp(280.0f, 400.0f, (continental - 0.7f) / 0.3f);                           // extreme peaks (increased height)
}

// =====================================================
// CALCULATE STEEPNESS (SLOPE) AT A POSITION
// =====================================================
float calculateSteepness(int x, int z, int heightMap[CHUNK_SIZE_X][CHUNK_SIZE_Z]) {
    float maxDiff = 0.0f;
    int height = heightMap[x][z];

    // Check 4 cardinal directions for height differences
    if (x > 0) maxDiff = std::max(maxDiff, std::abs(float(height - heightMap[x - 1][z])));
    if (x < CHUNK_SIZE_X - 1) maxDiff = std::max(maxDiff, std::abs(float(height - heightMap[x + 1][z])));
    if (z > 0) maxDiff = std::max(maxDiff, std::abs(float(height - heightMap[x][z - 1])));
    if (z < CHUNK_SIZE_Z - 1) maxDiff = std::max(maxDiff, std::abs(float(height - heightMap[x][z + 1])));

    // Normalize steepness (0 = flat, 1 = very steep)
    return clampf(maxDiff / 15.0f, 0.0f, 1.0f);
}

// =====================================================
// TERRAIN GENERATION
// =====================================================
void TerrainGenerator::generateFlatTerrain(Chunk& chunk) {

    const int chunkWorldX = chunk.chunkX * CHUNK_SIZE_X;
    const int chunkWorldZ = chunk.chunkZ * CHUNK_SIZE_Z;

    static float densityField[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    int heightMap[CHUNK_SIZE_X][CHUNK_SIZE_Z];
    float biomeMap[CHUNK_SIZE_X][CHUNK_SIZE_Z];
    float steepnessMap[CHUNK_SIZE_X][CHUNK_SIZE_Z];
    bool isCaveExposed[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];

    // Initialize cave exposure tracking
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                isCaveExposed[x][y][z] = false;
            }
        }
    }

    // =====================================================
    // PASS 1: BIOME + BASE HEIGHT
    // =====================================================
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            float wx = float(chunkWorldX + x);
            float wz = float(chunkWorldZ + z);

            float continental = noise.perlinOctave2D(wx * 0.0006f, wz * 0.0006f, 4, 0.5f);
            biomeMap[x][z] = continental;
            heightMap[x][z] = int(getBaseHeight(continental));
        }
    }

    // =====================================================
    // PASS 2: DENSITY FIELD
    // =====================================================
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            float wx = float(chunkWorldX + x);
            float wz = float(chunkWorldZ + z);

            float baseH = float(heightMap[x][z]);
            float biome = biomeMap[x][z];

            float mountainNoiseScale = 0.0f;
            float mountainAmplitude = 0.0f;
            float ridgeSharpness = 2.0f;

            if (biome < -0.3f) { // plains
                mountainNoiseScale = 0.0f;
                mountainAmplitude = 0.0f;
            }
            else if (biome < 0.1f) { // hills
                mountainNoiseScale = 0.008f;
                mountainAmplitude = 10.0f;
            }
            else if (biome < 0.4f) { // foothills
                mountainNoiseScale = 0.012f;
                mountainAmplitude = 25.0f;
            }
            else if (biome < 0.7f) { // mountain range
                mountainNoiseScale = 0.002f;
                mountainAmplitude = 100.0f;
                ridgeSharpness = 2.5f;
            }
            else { // extreme peaks
                mountainNoiseScale = 0.004f;  // Even larger features
                mountainAmplitude = 200.0f;    // Increased amplitude
                ridgeSharpness = 3.5f;         // Sharper ridges
            }

            float ridgeBase = noise.perlinOctave2D(wx * mountainNoiseScale, wz * mountainNoiseScale, 5, 0.5f);
            float ridge = ridgeNoise(ridgeBase, ridgeSharpness);
            float mountainMask = clampf((biome - 0.0f) * 1.5f, 0.0f, 1.0f);

            // Add extra sharpness noise for dramatic cliffs
            float sharpnessNoise = noise.perlinOctave2D(wx * 0.003f, wz * 0.003f, 3, 0.6f);
            float sharpnessFactor = clampf(sharpnessNoise + 0.3f, 0.0f, 1.0f);

            float mountainHeight = ridge * mountainMask * mountainAmplitude * sharpnessFactor;

            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                float density = (baseH - float(y)) + mountainHeight * verticalFalloff(y, 360);

                // Reduced vertical warp to prevent floating artifacts
                float verticalWarp = noise.perlinOctave3D(wx * 0.01f, y * 0.02f, wz * 0.01f, 3, 0.5f);
                density += verticalWarp * 8.0f; // Reduced from 12.0f

                // Extra sharp peaks at high altitudes (with stronger falloff)
                if (y > 200 && mountainMask > 0.3f) {
                    float peakNoise = noise.perlin3D(wx * 0.03f, y * 0.04f, wz * 0.03f);
                    float peakFalloff = verticalFalloff(y, 360);
                    density += ridgeNoise(peakNoise, 3.5f) * 50.0f * peakFalloff;
                }

                densityField[x][y][z] = density;
            }
        }
    }

    // =====================================================
    // PASS 2.5: CALCULATE STEEPNESS MAP
    // =====================================================
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            steepnessMap[x][z] = calculateSteepness(x, z, heightMap);
        }
    }

    // =====================================================
    // PASS 3: CAVES + RAVINES (WITH SURFACE ENTRANCES)
    // =====================================================
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            float wx = float(chunkWorldX + x);
            float wz = float(chunkWorldZ + z);
            int surfaceY = heightMap[x][z];

            for (int y = 5; y < CHUNK_SIZE_Y - 5; y++) {
                if (densityField[x][y][z] <= 0) continue;

                // Distance from surface
                float depth = float(surfaceY - y);

                // Allow caves to reach surface (no minimum depth restriction)
                // but use depth-based probability
                float caveMask = clampf(depth / 20.0f, 0.2f, 1.0f); // Min 20% strength at surface

                // === WORM CAVES (3D tunnels) ===
                float wormA = noise.perlin3D(wx * 0.015f, y * 0.02f, wz * 0.015f);
                float wormB = noise.perlin3D(wx * 0.015f + 500.0f, y * 0.02f + 500.0f, wz * 0.015f + 500.0f);
                float wormActivation = (wormA * wormA) + (wormB * wormB);

                if (wormActivation < 0.012f * caveMask) {
                    densityField[x][y][z] = -1.0f;
                    // Mark if near surface for proper entrance detection
                    if (depth < 15.0f) {
                        isCaveExposed[x][y][z] = true;
                    }
                    continue;
                }

                // === CHEESE CAVES (large caverns, only deep underground) ===
                if (y < 60 && depth > 25.0f) {
                    float cheese = noise.perlinOctave3D(wx * 0.03f, y * 0.03f, wz * 0.03f, 3, 0.5f);
                    if (cheese > 0.65f) {
                        densityField[x][y][z] = -1.0f;
                        continue;
                    }
                }

                // === SPAGHETTI CAVES (thin winding tunnels) ===
                if (depth > 10.0f) {
                    float spaghettiA = noise.perlin3D(wx * 0.025f, y * 0.03f, wz * 0.025f);
                    float spaghettiB = noise.perlin3D(wx * 0.025f + 1000.0f, y * 0.03f, wz * 0.025f + 1000.0f);
                    float spaghetti = std::abs(spaghettiA) + std::abs(spaghettiB);

                    if (spaghetti < 0.15f * caveMask) {
                        densityField[x][y][z] = -1.0f;
                        if (depth < 15.0f) {
                            isCaveExposed[x][y][z] = true;
                        }
                    }
                }
            }
        }
    }

    // =====================================================
    // PASS 4: BLOCK PLACEMENT (NO GRASS IN CAVES)
    // =====================================================
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {

            // Find top solid block
            int topSolidY = -1;
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
                if (densityField[x][y][z] > 0) {
                    topSolidY = y;
                    break;
                }
            }

            for (int y = 0; y < CHUNK_SIZE_Y; y++) {

                // Air blocks
                if (densityField[x][y][z] <= 0) {
                    chunk.setBlock(x, y, z, BlockType::AIR);
                    continue;
                }

                // Bedrock at bottom
                if (y == 0) {
                    chunk.setBlock(x, y, z, BlockType::STONE);
                    continue;
                }

                // Check if this block is exposed to sky or cave opening
                bool isExposedToSky = (y == topSolidY);
                bool isExposedToCave = false;

                // Check if adjacent to air (cave wall)
                for (int dy = -1; dy <= 1 && !isExposedToCave; dy++) {
                    for (int dx = -1; dx <= 1 && !isExposedToCave; dx++) {
                        for (int dz = -1; dz <= 1 && !isExposedToCave; dz++) {
                            int ny = y + dy;
                            int nx = x + dx;
                            int nz = z + dz;

                            if (ny >= 0 && ny < CHUNK_SIZE_Y &&
                                nx >= 0 && nx < CHUNK_SIZE_X &&
                                nz >= 0 && nz < CHUNK_SIZE_Z) {
                                if (densityField[nx][ny][nz] <= 0 && isCaveExposed[nx][ny][nz]) {
                                    isExposedToCave = true;
                                }
                            }
                        }
                    }
                }

                // Count solid blocks above this position
                int solidAbove = 0;
                for (int dy = 1; dy <= 4 && (y + dy) < CHUNK_SIZE_Y; dy++) {
                    if (densityField[x][y + dy][z] > 0) {
                        solidAbove++;
                    }
                    else {
                        break;
                    }
                }

                // Get steepness and height for this column
                float steepness = steepnessMap[x][z];
                float heightFactor = clampf((float(y) - 120.0f) / 120.0f, 0.0f, 1.0f); // 0 at y=120, 1 at y=240+

                // Calculate stone exposure probability
                // Increases with both height and steepness
                float stoneExposure = (steepness * 0.6f) + (heightFactor * 0.4f);
                stoneExposure = clampf(stoneExposure, 0.0f, 1.0f);

                // Add some noise variation to stone exposure
                float wx = float(chunkWorldX + x);
                float wz = float(chunkWorldZ + z);
                float stoneNoise = noise.perlin2D(wx * 0.05f, wz * 0.05f);
                stoneExposure += stoneNoise * 0.2f;
                stoneExposure = clampf(stoneExposure, 0.0f, 1.0f);

                // Determine block type
                if (isExposedToSky) {
                    // True surface - mix of grass and stone based on steepness/height
                    if (y > 240 || stoneExposure > 0.65f) {
                        // High peaks or very steep/exposed areas = stone
                        chunk.setBlock(x, y, z, BlockType::STONE);
                    }
                    else {
                        // Normal terrain = grass
                        chunk.setBlock(x, y, z, BlockType::GRASS);
                    }
                }
                else if (isExposedToCave && solidAbove == 0) {
                    // Cave entrance surface - use dirt or stone, NO GRASS
                    if (y > 220) {
                        chunk.setBlock(x, y, z, BlockType::STONE);
                    }
                    else {
                        chunk.setBlock(x, y, z, BlockType::DIRT);
                    }
                }
                else if (solidAbove > 0 && solidAbove <= 3) {
                    // Underground layer below surface = dirt
                    chunk.setBlock(x, y, z, BlockType::DIRT);
                }
                else {
                    // Everything else is stone
                    chunk.setBlock(x, y, z, BlockType::STONE);
                }
            }
        }
    }
}