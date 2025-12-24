#include "Rendering/Lighting.h"
#include <iostream>
#include <cmath>

Lighting::Lighting()
    : shadowFBO(0), shadowMap(0),
    shadowWidth(4096), shadowHeight(4096),
    timeOfDay(0.5f)  // Start at noon
{
    updateSunMoonPosition();
    updateColors();
}

Lighting::~Lighting() {
    if (shadowFBO) glDeleteFramebuffers(1, &shadowFBO);
    if (shadowMap) glDeleteTextures(1, &shadowMap);
}

void Lighting::initialize(int shadowWidth, int shadowHeight) {
    this->shadowWidth = shadowWidth;
    this->shadowHeight = shadowHeight;

    // Create shadow map framebuffer
    glGenFramebuffers(1, &shadowFBO);

    // Create shadow map texture
    glGenTextures(1, &shadowMap);
    glBindTexture(GL_TEXTURE_2D, shadowMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
        shadowWidth, shadowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // Attach shadow map to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Shadow framebuffer is not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::cout << "Shadow mapping initialized (" << shadowWidth << "x" << shadowHeight << ")" << std::endl;
}

void Lighting::setTimeOfDay(float time) {
    timeOfDay = std::fmod(time, 1.0f);
    if (timeOfDay < 0.0f) timeOfDay += 1.0f;
    updateSunMoonPosition();
    updateColors();
}

void Lighting::updateDayNightCycle(float deltaTime, float speed) {
    timeOfDay += deltaTime * speed;
    if (timeOfDay >= 1.0f) timeOfDay -= 1.0f;
    updateSunMoonPosition();
    updateColors();
}

void Lighting::updateSunMoonPosition() {
    // Convert time to angle (0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset)
    float angle = timeOfDay * 2.0f * 3.14159265f;

    // Sun position (rises in east, sets in west)
    sunDirection = glm::vec3(
        std::cos(angle) * 0.7f,  // East-West
        std::sin(angle),          // Up-Down
        0.3f                      // Slight north bias (2PM angle)
    );
    sunDirection = glm::normalize(sunDirection);

    // Moon is opposite to sun
    moonDirection = -sunDirection;
}

void Lighting::updateColors() {
    // Dawn: 0.2-0.3, Day: 0.3-0.7, Dusk: 0.7-0.8, Night: 0.8-0.2

    float sunHeight = sunDirection.y;

    if (timeOfDay >= 0.2f && timeOfDay <= 0.3f) {
        // Sunrise (orange/pink sky)
        float t = (timeOfDay - 0.2f) / 0.1f;
        skyColor = glm::mix(glm::vec3(0.1f, 0.1f, 0.2f), glm::vec3(1.0f, 0.6f, 0.3f), t);
        sunColor = glm::mix(glm::vec3(1.0f, 0.4f, 0.2f), glm::vec3(1.0f, 1.0f, 0.9f), t);
        ambientStrength = glm::mix(0.4f, 0.8f, t);  // CHANGED: was 0.2f to 0.5f
    }
    else if (timeOfDay > 0.3f && timeOfDay < 0.7f) {
        // Daytime (bright blue sky)
        skyColor = glm::vec3(0.53f, 0.81f, 0.98f);  // Sky blue
        sunColor = glm::vec3(1.0f, 0.98f, 0.92f);   // Warm white
        ambientStrength = 0.8f;  // CHANGED: was 0.6f - much brighter!
    }
    else if (timeOfDay >= 0.7f && timeOfDay <= 0.8f) {
        // Sunset (orange/red sky)
        float t = (timeOfDay - 0.7f) / 0.1f;
        skyColor = glm::mix(glm::vec3(1.0f, 0.5f, 0.2f), glm::vec3(0.1f, 0.1f, 0.2f), t);
        sunColor = glm::mix(glm::vec3(1.0f, 0.5f, 0.3f), glm::vec3(0.3f, 0.3f, 0.5f), t);
        ambientStrength = glm::mix(0.8f, 0.3f, t);  // CHANGED: was 0.5f to 0.2f
    }
    else {
        // Night (dark blue sky, moonlight)
        skyColor = glm::vec3(0.02f, 0.02f, 0.1f);   // Very dark blue
        sunColor = glm::vec3(0.0f, 0.0f, 0.0f);     // No sunlight
        ambientStrength = 0.25f;  // CHANGED: was 0.15f - slightly brighter nights
    }

    // Moon color (bluish-white)
    moonColor = glm::vec3(0.6f, 0.6f, 0.8f);

    // Boost ambient at night slightly
    if (sunHeight < 0.0f) {
        ambientStrength = std::max(0.25f, ambientStrength);  // CHANGED: was 0.15f
    }
}

void Lighting::beginShadowMapPass() {
    glViewport(0, 0, shadowWidth, shadowHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Set up light space matrix
    float orthoSize = 100.0f;
    glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 1.0f, 300.0f);

    glm::vec3 lightPos = -sunDirection * 150.0f;  // Position sun far away
    glm::mat4 lightView = glm::lookAt(
        lightPos,
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    lightSpaceMatrix = lightProjection * lightView;
}

void Lighting::endShadowMapPass() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Lighting::applyToShader(unsigned int shaderID, const glm::vec3& cameraPos) {
    // Determine if sun or moon
    bool isDay = sunDirection.y > 0.0f;
    glm::vec3 lightDir = isDay ? sunDirection : moonDirection;
    glm::vec3 lightColor = isDay ? sunColor : moonColor;

    // Send uniforms
    glUniform3f(glGetUniformLocation(shaderID, "sunDirection"), lightDir.x, lightDir.y, lightDir.z);
    glUniform3f(glGetUniformLocation(shaderID, "sunColor"), lightColor.r, lightColor.g, lightColor.b);
    glUniform3f(glGetUniformLocation(shaderID, "skyColor"), skyColor.r, skyColor.g, skyColor.b);
    glUniform1f(glGetUniformLocation(shaderID, "ambientStrength"), ambientStrength);
    glUniform3f(glGetUniformLocation(shaderID, "viewPos"), cameraPos.x, cameraPos.y, cameraPos.z);
    glUniform1i(glGetUniformLocation(shaderID, "isDay"), isDay ? 1 : 0);

    // Shadow map
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "lightSpaceMatrix"), 1, GL_FALSE, &lightSpaceMatrix[0][0]);
    glUniform1i(glGetUniformLocation(shaderID, "shadowMap"), 1);  // Tell shader shadowMap is on unit 1

    // Bind shadow map to texture unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowMap);

    // IMPORTANT: Set ourTexture to use texture unit 0
    glUniform1i(glGetUniformLocation(shaderID, "ourTexture"), 0);

    // Reset to texture unit 0 for subsequent texture bindings
    glActiveTexture(GL_TEXTURE0);
}