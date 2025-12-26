#ifndef LIGHTING_H
#define LIGHTING_H

#include <glad/glad.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

class Lighting {
public:
    Lighting();
    ~Lighting();

    void initialize(int shadowWidth = 4096, int shadowHeight = 4096);

    void setTimeOfDay(float time);
    void updateDayNightCycle(float deltaTime, float speed = 0.01f);

    void beginShadowMapPass();
    void endShadowMapPass();

    void applyToShader(unsigned int shaderID, const glm::vec3& cameraPos);

    // Getters
    glm::mat4 getLightSpaceMatrix() const { return lightSpaceMatrix; }
    unsigned int getShadowMap() const { return shadowMap; }
    float getTimeOfDay() const { return timeOfDay; }
    glm::vec3 getSunDirection() const { return sunDirection; }
    glm::vec3 getMoonDirection() const { return moonDirection; }
    glm::vec3 getSkyColor() const { return skyColor; }
    unsigned char getSkyLightLevel() const { return skyLightLevel; }

private:
    // Shadow mapping
    unsigned int shadowFBO;
    unsigned int shadowMap;
    int shadowWidth, shadowHeight;
    glm::mat4 lightSpaceMatrix;

    // Day/night cycle
    float timeOfDay;  // 0.0 to 1.0
    unsigned char skyLightLevel = 15;

    // Sun/Moon properties
    glm::vec3 sunDirection;
    glm::vec3 sunColor;
    glm::vec3 moonDirection;
    glm::vec3 moonColor;
    glm::vec3 skyColor;
    float ambientStrength;

    void updateSunMoonPosition();
    void updateColors();
};

#endif