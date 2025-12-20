#ifndef NOISE_H
#define NOISE_H

#include <vector>
#include <cstdint>

class Noise {
public:
    Noise(uint32_t seed = 0);

    // Perlin Noise (good for terrain)
    float perlin2D(float x, float y) const;
    float perlinOctave2D(float x, float y, int octaves, float persistence) const;

    float perlin3D(float x, float y, float z) const;
    float perlinOctave3D(float x, float y, float z, int octaves, float persistence) const;

    // Simplex Noise (better for caves - less grid artifacts)
    float simplex2D(float x, float y) const;
    float simplex3D(float x, float y, float z) const;
    float simplexOctave3D(float x, float y, float z, int octaves, float persistence) const;

    // Worley/Cellular Noise (for cheese caves)
    float worley3D(float x, float y, float z) const;

private:
    std::vector<int> permutation;
    uint32_t seed;

    // Perlin helpers
    float fade(float t) const;
    float lerp(float t, float a, float b) const;
    float grad2D(int hash, float x, float y) const;
    float grad3D(int hash, float x, float y, float z) const;

    // Simplex helpers
    float dot2D(const int* g, float x, float y) const;
    float dot3D(const int* g, float x, float y, float z) const;

    // Worley helpers
    float hash(float x, float y, float z) const;
    float noise3(float x, float y, float z) const;
};

#endif