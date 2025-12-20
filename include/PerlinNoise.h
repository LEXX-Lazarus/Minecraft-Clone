#pragma once
#include <vector>
#include <cstdint>

class PerlinNoise {
public:
    PerlinNoise(uint32_t seed);

    float noise(float x, float y) const;          // 2D noise
    float noise(float x, float y, float z) const; // 3D noise
    float octaveNoise(float x, float y, int octaves, float persistence) const;

private:
    std::vector<int> permutation;

    float fade(float t) const;
    float lerp(float t, float a, float b) const;
    float grad(int hash, float x, float y) const;
    float grad(int hash, float x, float y, float z) const; // 3D gradient
};
