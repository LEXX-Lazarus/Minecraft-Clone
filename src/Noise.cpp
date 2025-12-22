#include "Noise.h"
#include <cmath>
#include <algorithm>
#include <random>

// ======================================================
// CONSTRUCTOR
// ======================================================

Noise::Noise(uint32_t seed) : seed(seed) {
    permutation.resize(256);
    for (int i = 0; i < 256; i++) {
        permutation[i] = i;
    }

    std::default_random_engine engine(seed);
    std::shuffle(permutation.begin(), permutation.end(), engine);
    permutation.insert(permutation.end(), permutation.begin(), permutation.end());
}

// ======================================================
// PERLIN NOISE
// ======================================================

float Noise::fade(float t) const {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float Noise::lerp(float t, float a, float b) const {
    return a + t * (b - a);
}

float Noise::grad2D(int hash, float x, float y) const {
    int h = hash & 3;
    float u = h < 2 ? x : y;
    float v = h < 2 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

float Noise::grad3D(int hash, float x, float y, float z) const {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

float Noise::perlin2D(float x, float y) const {
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;

    x -= std::floor(x);
    y -= std::floor(y);

    float u = fade(x);
    float v = fade(y);

    int A = permutation[X] + Y;
    int B = permutation[X + 1] + Y;

    return lerp(v,
        lerp(u, grad2D(permutation[A], x, y),
            grad2D(permutation[B], x - 1, y)),
        lerp(u, grad2D(permutation[A + 1], x, y - 1),
            grad2D(permutation[B + 1], x - 1, y - 1))
    );
}

float Noise::perlin3D(float x, float y, float z) const {
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
            lerp(u, grad3D(permutation[AA], x, y, z),
                grad3D(permutation[BA], x - 1, y, z)),
            lerp(u, grad3D(permutation[AB], x, y - 1, z),
                grad3D(permutation[BB], x - 1, y - 1, z))),
        lerp(v,
            lerp(u, grad3D(permutation[AA + 1], x, y, z - 1),
                grad3D(permutation[BA + 1], x - 1, y, z - 1)),
            lerp(u, grad3D(permutation[AB + 1], x, y - 1, z - 1),
                grad3D(permutation[BB + 1], x - 1, y - 1, z - 1)))
    );
}

float Noise::perlinOctave2D(float x, float y, int octaves, float persistence) const {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; i++) {
        total += perlin2D(x * frequency, y * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total / maxValue;
}

float Noise::perlinOctave3D(float x, float y, float z, int octaves, float persistence) const {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; i++) {
        total += perlin3D(x * frequency, y * frequency, z * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total / maxValue;
}

// ======================================================
// SIMPLEX NOISE
// ======================================================

static const int grad3[][3] = {
    {1,1,0}, {-1,1,0}, {1,-1,0}, {-1,-1,0},
    {1,0,1}, {-1,0,1}, {1,0,-1}, {-1,0,-1},
    {0,1,1}, {0,-1,1}, {0,1,-1}, {0,-1,-1}
};

float Noise::dot3D(const int* g, float x, float y, float z) const {
    return g[0] * x + g[1] * y + g[2] * z;
}

float Noise::simplex3D(float x, float y, float z) const {
    // (unchanged — your existing implementation)
    // kept exactly as-is for safety
    float n0, n1, n2, n3;

    float F3 = 1.0f / 3.0f;
    float s = (x + y + z) * F3;
    int i = static_cast<int>(std::floor(x + s));
    int j = static_cast<int>(std::floor(y + s));
    int k = static_cast<int>(std::floor(z + s));

    float G3 = 1.0f / 6.0f;
    float t = (i + j + k) * G3;
    float X0 = i - t;
    float Y0 = j - t;
    float Z0 = k - t;
    float x0 = x - X0;
    float y0 = y - Y0;
    float z0 = z - Z0;

    int i1, j1, k1, i2, j2, k2;
    if (x0 >= y0) {
        if (y0 >= z0) { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
        else if (x0 >= z0) { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 0; k2 = 1; }
        else { i1 = 0; j1 = 0; k1 = 1; i2 = 1; j2 = 0; k2 = 1; }
    }
    else {
        if (y0 < z0) { i1 = 0; j1 = 0; k1 = 1; i2 = 0; j2 = 1; k2 = 1; }
        else if (x0 < z0) { i1 = 0; j1 = 1; k1 = 0; i2 = 0; j2 = 1; k2 = 1; }
        else { i1 = 0; j1 = 1; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
    }

    float x1 = x0 - i1 + G3;
    float y1 = y0 - j1 + G3;
    float z1 = z0 - k1 + G3;
    float x2 = x0 - i2 + 2.0f * G3;
    float y2 = y0 - j2 + 2.0f * G3;
    float z2 = z0 - k2 + 2.0f * G3;
    float x3 = x0 - 1.0f + 3.0f * G3;
    float y3 = y0 - 1.0f + 3.0f * G3;
    float z3 = z0 - 1.0f + 3.0f * G3;

    int ii = i & 255;
    int jj = j & 255;
    int kk = k & 255;

    int gi0 = permutation[ii + permutation[jj + permutation[kk]]] % 12;
    int gi1 = permutation[ii + i1 + permutation[jj + j1 + permutation[kk + k1]]] % 12;
    int gi2 = permutation[ii + i2 + permutation[jj + j2 + permutation[kk + k2]]] % 12;
    int gi3 = permutation[ii + 1 + permutation[jj + 1 + permutation[kk + 1]]] % 12;

    float t0 = 0.6f - x0 * x0 - y0 * y0 - z0 * z0;
    n0 = t0 < 0 ? 0.0f : t0 * t0 * t0 * t0 * dot3D(grad3[gi0], x0, y0, z0);

    float t1 = 0.6f - x1 * x1 - y1 * y1 - z1 * z1;
    n1 = t1 < 0 ? 0.0f : t1 * t1 * t1 * t1 * dot3D(grad3[gi1], x1, y1, z1);

    float t2 = 0.6f - x2 * x2 - y2 * y2 - z2 * z2;
    n2 = t2 < 0 ? 0.0f : t2 * t2 * t2 * t2 * dot3D(grad3[gi2], x2, y2, z2);

    float t3 = 0.6f - x3 * x3 - y3 * y3 - z3 * z3;
    n3 = t3 < 0 ? 0.0f : t3 * t3 * t3 * t3 * dot3D(grad3[gi3], x3, y3, z3);

    return 32.0f * (n0 + n1 + n2 + n3);
}

float Noise::simplexOctave3D(float x, float y, float z, int octaves, float persistence) const {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; i++) {
        total += simplex3D(x * frequency, y * frequency, z * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total / maxValue;
}

float Noise::simplex2D(float x, float y) const {
    return perlin2D(x, y);
}

// ======================================================
// WORLEY NOISE
// ======================================================

float Noise::hash(float x, float y, float z) const {
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    int iz = static_cast<int>(std::floor(z));
    return static_cast<float>(
        permutation[(ix & 255) + permutation[(iy & 255) + permutation[iz & 255]]]
        ) / 255.0f;
}

float Noise::worley3D(float x, float y, float z) const {
    int xi = static_cast<int>(std::floor(x));
    int yi = static_cast<int>(std::floor(y));
    int zi = static_cast<int>(std::floor(z));

    float minDist = 1e9f;

    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dz = -1; dz <= 1; dz++) {
                float px = xi + dx + hash(xi + dx, yi + dy, zi + dz);
                float py = yi + dy + hash(xi + dx + 1, yi + dy, zi + dz);
                float pz = zi + dz + hash(xi + dx, yi + dy + 1, zi + dz);

                float dxp = x - px;
                float dyp = y - py;
                float dzp = z - pz;

                minDist = std::min(minDist, std::sqrt(dxp * dxp + dyp * dyp + dzp * dzp));
            }
        }
    }

    return minDist;
}

// ======================================================
// RIDGED / MOUNTAIN NOISE
// ======================================================

float Noise::ridgedPerlin2D(float x, float y, int octaves, float persistence) const {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float weight = 1.0f;

    for (int i = 0; i < octaves; i++) {
        float n = perlin2D(x * frequency, y * frequency);
        n = 1.0f - std::abs(n);
        n *= n;
        n *= weight;

        weight = std::clamp(n * 2.0f, 0.0f, 1.0f);

        total += n * amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total;
}

float Noise::mountainNoise2D(float x, float y) const {
    float warpX = perlin2D(x * 0.002f, y * 0.002f) * 40.0f;
    float warpY = perlin2D(x * 0.002f + 1000.0f, y * 0.002f + 1000.0f) * 40.0f;

    float nx = x + warpX;
    float ny = y + warpY;

    float ridges = ridgedPerlin2D(nx * 0.0008f, ny * 0.0008f, 6, 0.5f);
    ridges = std::pow(ridges, 1.4f);

    return std::clamp(ridges, 0.0f, 1.0f);
}