#ifndef LIGHTING_H
#define LIGHTING_H

#include <glm.hpp>

class Lighting {
public:
    Lighting();

    // Time control (0.0–1.0)
    void setTimeOfDay(float t);
    void update(float deltaTime, float speed = 0.01f);

    // Outputs
    unsigned char getSkyLightLevel() const { return skyLightLevel; }
    const glm::vec3& getSkyColor() const { return skyColor; }
    float getTimeOfDay() const { return timeOfDay; }

private:
    void updateSky();

private:
    float timeOfDay;              // 0–1
    unsigned char skyLightLevel;  // 0–15
    glm::vec3 skyColor;           // Skybox only
};

#endif
