#ifndef LIGHTING_H
#define LIGHTING_H

#include <glad/glad.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

class Lighting {
public:
    Lighting();
    ~Lighting();

    // Initialize shadow map framebuffer
    void initialize(int shadowWidth = 4096, int shadowHeight = 4096);

    // Update time of day (0.0 = midnight, 0.5 = noon, 1.0 = midnight)
    void setTimeOfDay(float time);
    void updateDayNightCycle(float deltaTime, float speed = 0.01f);

    // Shadow map pass (call BEFORE main render)
    void beginShadowMapPass();
    void endShadowMapPass();

    // Apply lighting to main shader
    void applyToShader(unsigned int shaderID, const glm::vec3& cameraPos);

    // Getters
    glm::mat4 getLightSpaceMatrix() const { return lightSpaceMatrix; }
    unsigned int getShadowMap() const { return shadowMap; }
    float getTimeOfDay() const { return timeOfDay; }

private:
    // Shadow mapping
    unsigned int shadowFBO;
    unsigned int shadowMap;
    int shadowWidth, shadowHeight;
    glm::mat4 lightSpaceMatrix;

    // Day/night cycle
    float timeOfDay;  // 0.0 to 1.0

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