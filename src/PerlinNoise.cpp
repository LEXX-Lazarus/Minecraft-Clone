#include "PerlinNoise.h"
#include <cmath>
#include <algorithm>
#include <random>

PerlinNoise::PerlinNoise(uint32_t seed) {
    permutation.resize(256);
    for (int i = 0; i < 256; i++) permutation[i] = i;

    std::default_random_engine engine(seed);
    std::shuffle(permutation.begin(), permutation.end(), engine);

    permutation.insert(permutation.end(), permutation.begin(), permutation.end());
}

float PerlinNoise::fade(float t) const {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float PerlinNoise::lerp(float t, float a, float b) const {
    return a + t * (b - a);
}

// 2D gradient
float PerlinNoise::grad(int hash, float x, float y) const {
    int h = hash & 3;
    float u = h < 2 ? x : y;
    float v = h < 2 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

// 3D gradient
float PerlinNoise::grad(int hash, float x, float y, float z) const {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

// 2D noise
float PerlinNoise::noise(float x, float y) const {
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;

    x -= std::floor(x);
    y -= std::floor(y);

    float u = fade(x);
    float v = fade(y);

    int A = permutation[X] + Y;
    int B = permutation[X + 1] + Y;

    return lerp(v,
        lerp(u, grad(permutation[A], x, y),
            grad(permutation[B], x - 1, y)),
        lerp(u, grad(permutation[A + 1], x, y - 1),
            grad(permutation[B + 1], x - 1, y - 1))
    );
}

// 3D noise
float PerlinNoise::noise(float x, float y, float z) const {
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;
    int Z = static_cast<int>(std::floor(z)) & 255;

    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);

    float u = fade(x);
    float v = fade(y);
    float w = fade(z);

    int A = permutation[X] + Y;
    int AA = permutation[A] + Z;
    int AB = permutation[A + 1] + Z;
    int B = permutation[X + 1] + Y;
    int BA = permutation[B] + Z;
    int BB = permutation[B + 1] + Z;

    return lerp(w,
        lerp(v,
            lerp(u, grad(permutation[AA], x, y, z),
                grad(permutation[BA], x - 1, y, z)),
            lerp(u, grad(permutation[AB], x, y - 1, z),
                grad(permutation[BB], x - 1, y - 1, z))
        ),
        lerp(v,
            lerp(u, grad(permutation[AA + 1], x, y, z - 1),
                grad(permutation[BA + 1], x - 1, y, z - 1)),
            lerp(u, grad(permutation[AB + 1], x, y - 1, z - 1),
                grad(permutation[BB + 1], x - 1, y - 1, z - 1))
        )
    );
}

// Octave noise 2D
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
