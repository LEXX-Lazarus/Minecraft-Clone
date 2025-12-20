#ifndef PERLIN_NOISE_H
#define PERLIN_NOISE_H

#include <vector>
#include <cstdint>

class PerlinNoise {
public:
    PerlinNoise(uint32_t seed = 0);

    // Get noise value at coordinates (returns value between -1 and 1)
    float noise(float x, float y) const;

    // Get octave noise (multiple layers for more natural look)
    float octaveNoise(float x, float y, int octaves, float persistence) const;

private:
    std::vector<int> permutation;

    float fade(float t) const;
    float lerp(float t, float a, float b) const;
    float grad(int hash, float x, float y) const;
};

#endif