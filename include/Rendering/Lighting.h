#ifndef LIGHTING_H
#define LIGHTING_H

#include "../external/glm/glm.hpp"

class Lighting {
public:
    Lighting();
    ~Lighting() = default;

    // Get directional light direction (sun)
    glm::vec3 getSunDirection() const { return sunDirection; }

    // Get sun color
    glm::vec3 getSunColor() const { return sunColor; }

    // Get ambient light level
    float getAmbientStrength() const { return ambientStrength; }

    // Apply lighting to shader
    void applyToShader(unsigned int shaderID);

    // Optional: Set time of day (0.0 = midnight, 0.5 = noon, 1.0 = midnight)
    void setTimeOfDay(float time);

private:
    glm::vec3 sunDirection;      // Direction TO the sun
    glm::vec3 sunColor;          // Color of sunlight
    float ambientStrength;       // Base ambient light level

    void updateSunAngle();
};

#endif