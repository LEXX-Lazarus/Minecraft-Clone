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
    // Old: float angle = timeOfDay * 2.0f * 3.14159265f;
    // New:
    float angle = (timeOfDay * 2.0f * 3.14159265f) - (3.14159265f / 2.0f);

    // Sun position (rises in east, sets in west)
    sunDirection = glm::vec3(
        std::cos(angle) * 0.7f,  // East-West
        std::sin(angle),          // Up-Down
        0.3f                      // Slight north bias
    );
    sunDirection = glm::normalize(sunDirection);

    // Moon is opposite to sun
    moonDirection = -sunDirection;
}

void Lighting::updateColors() {
    // Convert timeOfDay (0-1) to hours (0-24)
    float hours = timeOfDay * 24.0f;

    // -----------------------------
    // Color palette - VIBRANT AND SATURATED
    // (MIDDAY FIX: less cyan, NIGHT DARKER, LONGER ORANGE)
    // -----------------------------
    glm::vec3 midnightSky(0.02f, 0.04f, 0.12f);        // Deep dark night
    glm::vec3 predawnSky(0.05f, 0.08f, 0.25f);         // Dark blue pre-sunrise
    glm::vec3 sunriseSky(1.0f, 0.5f, 0.2f);            // Orange sunrise
    glm::vec3 morningSky(0.30f, 0.55f, 0.95f);         // Morning blue
    glm::vec3 noonSky(0.25f, 0.55f, 0.98f);            // True blue
    glm::vec3 afternoonSky(0.35f, 0.65f, 0.95f);       // Afternoon warm
    glm::vec3 sunsetSky(1.0f, 0.4f, 0.15f);            // Orange/red sunset
    glm::vec3 duskSky(0.12f, 0.16f, 0.35f);            // Purple dusk

    glm::vec3 nightLight(0.05f, 0.08f, 0.18f);         // Dark moonlight
    glm::vec3 predawnLight(0.2f, 0.25f, 0.4f);         // Pre-sunrise glow
    glm::vec3 sunriseLight(1.0f, 0.6f, 0.3f);          // Warm orange light
    glm::vec3 morningLight(1.0f, 0.95f, 0.85f);        // Morning white
    glm::vec3 noonLight(1.0f, 1.0f, 0.95f);            // Noon white
    glm::vec3 afternoonLight(1.0f, 0.95f, 0.8f);       // Afternoon warm
    glm::vec3 sunsetLight(1.0f, 0.5f, 0.2f);           // Sunset orange
    glm::vec3 duskLight(0.35f, 0.4f, 0.65f);           // Purple dusk

    // -----------------------------
    // Helper lambda: Saturation-preserving interpolation
    // -----------------------------
    auto mixSaturated = [](const glm::vec3& a, const glm::vec3& b, float t) {
        glm::vec3 mixed = glm::mix(a, b, t);
        float luminance = glm::dot(mixed, glm::vec3(0.2126f, 0.7152f, 0.0722f));
        glm::vec3 chroma = mixed - glm::vec3(luminance);
        mixed = glm::vec3(luminance) + chroma * 1.08f;
        return glm::clamp(mixed, 0.0f, 1.0f);
        };

    // -----------------------------
    // Smooth interpolation based on time of day
    // -----------------------------
    if (hours >= 0.0f && hours < 5.0f) {
        // 12 AM - 5 AM: Dark night
        float t = glm::smoothstep(0.0f, 5.0f, hours);
        skyColor = mixSaturated(midnightSky, predawnSky, t);
        sunColor = glm::mix(nightLight, predawnLight, t);
        ambientStrength = glm::mix(0.05f, 0.12f, t);
        skyLightLevel = static_cast<unsigned char>(glm::mix(1.0f, 2.0f, t));  // LL 1-2
    }
    else if (hours >= 5.0f && hours < 6.5f) {
        // 5 AM - 6:30 AM: Extended orange sunrise
        float t = glm::smoothstep(5.0f, 6.5f, hours);
        skyColor = mixSaturated(predawnSky, sunriseSky, t);
        sunColor = glm::mix(predawnLight, sunriseLight, t);
        ambientStrength = glm::mix(0.12f, 0.45f, t);
        skyLightLevel = static_cast<unsigned char>(glm::mix(2.0f, 8.0f, t));  // LL 2-8
    }
    else if (hours >= 6.5f && hours < 9.5f) {
        // 6:30 AM - 9:30 AM: Gradual transition from orange to morning blue
        float t = glm::smoothstep(6.5f, 9.5f, hours);
        skyColor = mixSaturated(sunriseSky, morningSky, t);
        sunColor = glm::mix(sunriseLight, morningLight, t);
        ambientStrength = glm::mix(0.45f, 0.75f, t);
        skyLightLevel = static_cast<unsigned char>(glm::mix(8.0f, 15.0f, t));  // LL 8-15
    }
    else if (hours >= 9.5f && hours < 12.0f) {
        // 9:30 AM - 12 PM: Morning -> Noon blue
        float t = glm::smoothstep(9.5f, 12.0f, hours);
        skyColor = mixSaturated(morningSky, noonSky, t);
        sunColor = glm::mix(morningLight, noonLight, t);
        ambientStrength = glm::mix(0.75f, 0.9f, t);
        skyLightLevel = 15;  // LL 15-15
    }
    else if (hours >= 12.0f && hours < 17.0f) {
        // 12 PM - 5 PM: Afternoon blue, slowly darkening
        float t = glm::smoothstep(12.0f, 17.0f, hours);
        skyColor = mixSaturated(noonSky, afternoonSky, t);
        sunColor = glm::mix(noonLight, afternoonLight, t);
        ambientStrength = glm::mix(0.9f, 0.6f, t);
        skyLightLevel = 15;  // LL 15-15
    }
    else if (hours >= 17.0f && hours < 20.0f) {
        // 5 PM - 8 PM: Extended orange sunset
        float t = glm::smoothstep(17.0f, 20.0f, hours);
        skyColor = mixSaturated(afternoonSky, sunsetSky, t);
        sunColor = glm::mix(afternoonLight, sunsetLight, t);
        ambientStrength = glm::mix(0.6f, 0.35f, t);
        skyLightLevel = static_cast<unsigned char>(glm::mix(14.0f, 8.0f, t));  // LL 15-8
    }
    else if (hours >= 20.0f && hours < 21.0f) {
        // 8 PM - 9 PM: Dusk transition
        float t = glm::smoothstep(20.0f, 21.0f, hours);
        skyColor = mixSaturated(sunsetSky, duskSky, t);
        sunColor = glm::mix(sunsetLight, duskLight, t);
        ambientStrength = glm::mix(0.35f, 0.12f, t);
        skyLightLevel = static_cast<unsigned char>(glm::mix(8.0f, 2.0f, t));  // LL 8-2
    }
    else {
        // 9 PM - 12 AM: Night
        float t = glm::smoothstep(21.0f, 24.0f, hours);
        skyColor = mixSaturated(duskSky, midnightSky, t);
        sunColor = glm::mix(duskLight, nightLight, t);
        ambientStrength = 0.05f;
        skyLightLevel = static_cast<unsigned char>(glm::mix(2.0f, 1.0f, t));  // LL 2-1
    }

    // -----------------------------
    // Conditional gamma correction to avoid gray night
    // -----------------------------
    float skyLuma = glm::dot(skyColor, glm::vec3(0.2126f, 0.7152f, 0.0722f));
    float gamma = glm::mix(1.0f, 1.0f / 2.2f, glm::clamp(skyLuma * 1.4f, 0.0f, 1.0f));
    skyColor = glm::pow(skyColor, glm::vec3(gamma));

    // Moon color (cool bluish-white)
    moonColor = glm::vec3(0.55f, 0.6f, 0.8f);
}

void Lighting::beginShadowMapPass() {
    glViewport(0, 0, shadowWidth, shadowHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Set up light space matrix
    float orthoSize = 100.0f;
    glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 1.0f, 300.0f);

    glm::vec3 lightPos = -sunDirection * 150.0f;
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
    glUniform1i(glGetUniformLocation(shaderID, "shadowMap"), 1);

    // Bind shadow map to texture unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowMap);

    // Set ourTexture to use texture unit 0
    glUniform1i(glGetUniformLocation(shaderID, "ourTexture"), 0);

    // Reset to texture unit 0
    glActiveTexture(GL_TEXTURE0);
}