#include "PerlinNoise.h"
#include <cmath>
#include <algorithm>
#include <random>

PerlinNoise::PerlinNoise(uint32_t seed) {
    // Initialize permutation table
    permutation.resize(256);
    for (int i = 0; i < 256; i++) {
        permutation[i] = i;
    }

    // Shuffle based on seed
    std::default_random_engine engine(seed);
    std::shuffle(permutation.begin(), permutation.end(), engine);

    // Duplicate for easy wrapping
    permutation.insert(permutation.end(), permutation.begin(), permutation.end());
}

float PerlinNoise::fade(float t) const {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float PerlinNoise::lerp(float t, float a, float b) const {
    return a + t * (b - a);
}

float PerlinNoise::grad(int hash, float x, float y) const {
    int h = hash & 3;
    float u = h < 2 ? x : y;
    float v = h < 2 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

float PerlinNoise::noise(float x, float y) const {
    // Find unit grid cell containing point
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;

    // Get relative xy coordinates within cell
    x -= std::floor(x);
    y -= std::floor(y);

    // Compute fade curves
    float u = fade(x);
    float v = fade(y);

    // Hash coordinates of the 4 square corners
    int A = permutation[X] + Y;
    int B = permutation[X + 1] + Y;

    // Blend results from the 4 corners
    return lerp(v,
        lerp(u, grad(permutation[A], x, y),
            grad(permutation[B], x - 1, y)),
        lerp(u, grad(permutation[A + 1], x, y - 1),
            grad(permutation[B + 1], x - 1, y - 1))
    );
}

float PerlinNoise::octaveNoise(float x, float y, int octaves, float persistence) const {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; i++) {
        total += noise(x * frequency, y * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total / maxValue;
}