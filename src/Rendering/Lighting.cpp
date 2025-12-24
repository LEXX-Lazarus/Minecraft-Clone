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
        0.3f                      // Slight north bias
    );
    sunDirection = glm::normalize(sunDirection);

    // Moon is opposite to sun
    moonDirection = -sunDirection;
}

void Lighting::updateColors() {
    // Convert timeOfDay (0-1) to hours (0-24)
    float hours = timeOfDay * 24.0f;

    // Define color palette - VIBRANT AND SATURATED
    glm::vec3 midnightSky(0.05f, 0.08f, 0.25f);        // Deep blue night (darkest)
    glm::vec3 predawnSky(0.08f, 0.12f, 0.35f);         // Dark blue before sunrise
    glm::vec3 sunriseSky(1.0f, 0.5f, 0.2f);            // Vibrant orange sunrise
    glm::vec3 morningSky(0.4f, 0.7f, 1.0f);            // Bright morning blue
    glm::vec3 noonSky(0.4f, 0.75f, 1.0f);              // Brightest blue sky
    glm::vec3 afternoonSky(0.5f, 0.75f, 0.95f);        // Slightly warmer afternoon
    glm::vec3 sunsetSky(1.0f, 0.4f, 0.15f);            // Deep orange/red sunset
    glm::vec3 duskSky(0.15f, 0.2f, 0.45f);             // Purple dusk

    glm::vec3 nightLight(0.15f, 0.2f, 0.35f);          // Dim bluish moonlight
    glm::vec3 predawnLight(0.3f, 0.35f, 0.5f);         // Pre-sunrise glow
    glm::vec3 sunriseLight(1.0f, 0.6f, 0.3f);          // Warm orange light
    glm::vec3 morningLight(1.0f, 0.95f, 0.85f);        // Bright warm white
    glm::vec3 noonLight(1.0f, 1.0f, 0.95f);            // Brightest white
    glm::vec3 afternoonLight(1.0f, 0.95f, 0.8f);       // Warm afternoon light
    glm::vec3 sunsetLight(1.0f, 0.5f, 0.2f);           // Orange sunset light
    glm::vec3 duskLight(0.4f, 0.45f, 0.7f);            // Purple dusk light

    // Smooth interpolation based on time of day
    if (hours >= 23.5f || hours < 0.5f) {
        // 11:30 PM - 12:30 AM: Darkest period (extended)
        skyColor = midnightSky;
        sunColor = nightLight;
        ambientStrength = 0.12f;  // Stay at darkest
    }
    else if (hours >= 0.5f && hours < 5.0f) {
        // 12:30 AM - 5 AM: Dark bluish night
        float t = glm::smoothstep(0.5f, 5.0f, hours);
        skyColor = glm::mix(midnightSky, predawnSky, t);
        sunColor = glm::mix(nightLight, predawnLight, t);
        ambientStrength = glm::mix(0.12f, 0.18f, t);
    }
    else if (hours >= 5.0f && hours < 6.0f) {
        // 5 AM - 6 AM: Orange starts to appear
        float t = glm::smoothstep(5.0f, 6.0f, hours);
        skyColor = glm::mix(predawnSky, sunriseSky, t);
        sunColor = glm::mix(predawnLight, sunriseLight, t);
        ambientStrength = glm::mix(0.18f, 0.45f, t);
    }
    else if (hours >= 6.0f && hours < 8.0f) {
        // 6 AM - 8 AM: Orange sunrise - smoother transition to blue
        float t = glm::smoothstep(6.0f, 8.0f, hours);
        // Create intermediate orange-blue blend to avoid grey
        glm::vec3 orangeBlue = glm::vec3(0.7f, 0.6f, 0.6f);  // Warm transition color
        skyColor = glm::mix(sunriseSky, orangeBlue, t);
        sunColor = glm::mix(sunriseLight, morningLight, t);
        ambientStrength = glm::mix(0.45f, 0.70f, t);
    }
    else if (hours >= 8.0f && hours < 10.0f) {
        // 8 AM - 10 AM: Transition to morning blue
        float t = glm::smoothstep(8.0f, 10.0f, hours);
        glm::vec3 orangeBlue = glm::vec3(0.7f, 0.6f, 0.6f);
        skyColor = glm::mix(orangeBlue, morningSky, t);
        sunColor = glm::mix(morningLight, noonLight, t * 0.5f);
        ambientStrength = glm::mix(0.70f, 0.85f, t);
    }
    else if (hours >= 10.0f && hours < 12.0f) {
        // 10 AM - 12 PM: Reach brightest noon
        float t = glm::smoothstep(10.0f, 12.0f, hours);
        skyColor = glm::mix(morningSky, noonSky, t);
        sunColor = glm::mix(noonLight, noonLight, t);
        ambientStrength = glm::mix(0.85f, 1.0f, t);
    }
    else if (hours >= 12.0f && hours < 16.0f) {
        // 12 PM - 4 PM: Bright blue afternoon (extended)
        float t = glm::smoothstep(12.0f, 16.0f, hours);
        skyColor = glm::mix(noonSky, afternoonSky, t);
        sunColor = glm::mix(noonLight, afternoonLight, t);
        ambientStrength = glm::mix(1.0f, 0.70f, t);
    }
    else if (hours >= 16.0f && hours < 17.5f) {
        // 4 PM - 5:30 PM: Start sunset earlier with smooth transition
        float t = glm::smoothstep(16.0f, 17.5f, hours);
        glm::vec3 blueOrange = glm::vec3(0.75f, 0.6f, 0.55f);  // Warm transition color
        skyColor = glm::mix(afternoonSky, blueOrange, t);
        sunColor = glm::mix(afternoonLight, sunsetLight, t);
        ambientStrength = glm::mix(0.70f, 0.50f, t);
    }
    else if (hours >= 17.5f && hours < 19.0f) {
        // 5:30 PM - 7 PM: Full orange sunset
        float t = glm::smoothstep(17.5f, 19.0f, hours);
        glm::vec3 blueOrange = glm::vec3(0.75f, 0.6f, 0.55f);
        skyColor = glm::mix(blueOrange, sunsetSky, t);
        sunColor = glm::mix(sunsetLight, sunsetLight, t);
        ambientStrength = glm::mix(0.50f, 0.35f, t);
    }
    else if (hours >= 19.0f && hours < 20.0f) {
        // 7 PM - 8 PM: Dusk transition
        float t = glm::smoothstep(19.0f, 20.0f, hours);
        skyColor = glm::mix(sunsetSky, duskSky, t);
        sunColor = glm::mix(sunsetLight, duskLight, t);
        ambientStrength = glm::mix(0.35f, 0.18f, t);
    }
    else {
        // 8 PM - 11:30 PM: Dark blue night
        float t = glm::smoothstep(20.0f, 23.5f, hours);
        skyColor = glm::mix(duskSky, midnightSky, t);
        sunColor = glm::mix(duskLight, nightLight, t);
        ambientStrength = glm::mix(0.18f, 0.12f, t);
    }

    // Moon color (cool bluish-white)
    moonColor = glm::vec3(0.6f, 0.65f, 0.85f);
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