#include "Rendering/Lighting.h"
#include <algorithm>
#include <cmath>

Lighting::Lighting()
    : timeOfDay(0.5f), skyLightLevel(15), skyColor(0.53f, 0.81f, 0.98f)
{
}

void Lighting::setTimeOfDay(float t) {
    timeOfDay = std::fmod(t, 1.0f);
    if (timeOfDay < 0.0f) timeOfDay += 1.0f;
    updateSky();
}

void Lighting::update(float deltaTime, float speed) {
    timeOfDay += deltaTime * speed;
    if (timeOfDay >= 1.0f) timeOfDay -= 1.0f;
    updateSky();
}

// Helper function for smooth interpolation
static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float smoothstep(float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

glm::vec3 lerpColor(const glm::vec3& a, const glm::vec3& b, float t) {
    return glm::vec3(
        lerp(a.r, b.r, t),
        lerp(a.g, b.g, t),
        lerp(a.b, b.b, t)
    );
}

// Saturation-preserving color mix
glm::vec3 mixSaturated(const glm::vec3& a, const glm::vec3& b, float t) {
    glm::vec3 mixed = lerpColor(a, b, t);
    float luminance = mixed.r * 0.2126f + mixed.g * 0.7152f + mixed.b * 0.0722f;
    glm::vec3 chroma = mixed - glm::vec3(luminance);
    mixed = glm::vec3(luminance) + chroma * 1.08f;
    return glm::vec3(
        std::clamp(mixed.r, 0.0f, 1.0f),
        std::clamp(mixed.g, 0.0f, 1.0f),
        std::clamp(mixed.b, 0.0f, 1.0f)
    );
}

void Lighting::updateSky() {
    float hours = timeOfDay * 24.0f;

    // -----------------------------
    // Color palette - VIBRANT AND SATURATED (matching reference)
    // -----------------------------
    glm::vec3 midnightSky(0.02f, 0.04f, 0.12f);        // Deep dark night
    glm::vec3 predawnSky(0.05f, 0.08f, 0.25f);         // Dark blue pre-sunrise
    glm::vec3 sunriseSky(1.0f, 0.5f, 0.2f);            // Orange sunrise
    glm::vec3 morningSky(0.30f, 0.55f, 0.95f);         // Morning blue
    glm::vec3 noonSky(0.25f, 0.55f, 0.98f);            // True blue
    glm::vec3 afternoonSky(0.35f, 0.65f, 0.95f);       // Afternoon warm
    glm::vec3 sunsetSky(1.0f, 0.4f, 0.15f);            // Orange/red sunset
    glm::vec3 duskSky(0.12f, 0.16f, 0.35f);            // Purple dusk

    // -----------------------------
    // SMOOTH SKY COLOR GRADIENTS (matching reference timing)
    // -----------------------------
    if (hours >= 0.0f && hours < 5.0f) {
        // 12 AM - 5 AM: Dark night
        float t = smoothstep(0.0f, 5.0f, hours);
        skyColor = mixSaturated(midnightSky, predawnSky, t);
    }
    else if (hours >= 5.0f && hours < 6.5f) {
        // 5 AM - 6:30 AM: Extended orange sunrise
        float t = smoothstep(5.0f, 6.5f, hours);
        skyColor = mixSaturated(predawnSky, sunriseSky, t);
    }
    else if (hours >= 6.5f && hours < 9.5f) {
        // 6:30 AM - 9:30 AM: Gradual transition from orange to morning blue
        float t = smoothstep(6.5f, 9.5f, hours);
        skyColor = mixSaturated(sunriseSky, morningSky, t);
    }
    else if (hours >= 9.5f && hours < 12.0f) {
        // 9:30 AM - 12 PM: Morning -> Noon blue
        float t = smoothstep(9.5f, 12.0f, hours);
        skyColor = mixSaturated(morningSky, noonSky, t);
    }
    else if (hours >= 12.0f && hours < 17.0f) {
        // 12 PM - 5 PM: Afternoon blue, slowly darkening
        float t = smoothstep(12.0f, 17.0f, hours);
        skyColor = mixSaturated(noonSky, afternoonSky, t);
    }
    else if (hours >= 17.0f && hours < 20.0f) {
        // 5 PM - 8 PM: Extended orange sunset
        float t = smoothstep(17.0f, 20.0f, hours);
        skyColor = mixSaturated(afternoonSky, sunsetSky, t);
    }
    else if (hours >= 20.0f && hours < 21.0f) {
        // 8 PM - 9 PM: Dusk transition
        float t = smoothstep(20.0f, 21.0f, hours);
        skyColor = mixSaturated(sunsetSky, duskSky, t);
    }
    else {
        // 9 PM - 12 AM: Night
        float t = smoothstep(21.0f, 24.0f, hours);
        skyColor = mixSaturated(duskSky, midnightSky, t);
    }

    // -----------------------------
    // Conditional gamma correction to avoid gray night
    // -----------------------------
    float skyLuma = skyColor.r * 0.2126f + skyColor.g * 0.7152f + skyColor.b * 0.0722f;
    float gamma = lerp(1.0f, 1.0f / 2.2f, std::clamp(skyLuma * 1.4f, 0.0f, 1.0f));
    skyColor = glm::vec3(
        std::pow(skyColor.r, gamma),
        std::pow(skyColor.g, gamma),
        std::pow(skyColor.b, gamma)
    );

    // -----------------------------
    // SMOOTH LIGHT LEVEL TRANSITIONS (matching reference timing)
    // -----------------------------
    if (hours >= 0.0f && hours < 5.0f) {
        // 12 AM - 5 AM: Dark night (LL 1-2)
        float t = smoothstep(0.0f, 5.0f, hours);
        float lightFloat = lerp(1.0f, 2.0f, t);
        skyLightLevel = static_cast<unsigned char>(std::round(lightFloat));
    }
    else if (hours >= 5.0f && hours < 6.5f) {
        // 5 AM - 6:30 AM: Extended orange sunrise (LL 2-8)
        float t = smoothstep(5.0f, 6.5f, hours);
        float lightFloat = lerp(2.0f, 8.0f, t);
        skyLightLevel = static_cast<unsigned char>(std::round(lightFloat));
    }
    else if (hours >= 6.5f && hours < 9.5f) {
        // 6:30 AM - 9:30 AM: Morning brightening (LL 8-15)
        float t = smoothstep(6.5f, 9.5f, hours);
        float lightFloat = lerp(8.0f, 15.0f, t);
        skyLightLevel = static_cast<unsigned char>(std::round(lightFloat));
    }
    else if (hours >= 9.5f && hours < 17.0f) {
        // 9:30 AM - 5 PM: Full daylight (LL 15)
        skyLightLevel = 15;
    }
    else if (hours >= 17.0f && hours < 20.0f) {
        // 5 PM - 8 PM: Extended orange sunset (LL 15-8)
        float t = smoothstep(17.0f, 20.0f, hours);
        float lightFloat = lerp(15.0f, 8.0f, t);
        skyLightLevel = static_cast<unsigned char>(std::round(lightFloat));
    }
    else if (hours >= 20.0f && hours < 21.0f) {
        // 8 PM - 9 PM: Dusk transition (LL 8-2)
        float t = smoothstep(20.0f, 21.0f, hours);
        float lightFloat = lerp(8.0f, 2.0f, t);
        skyLightLevel = static_cast<unsigned char>(std::round(lightFloat));
    }
    else {
        // 9 PM - 12 AM: Night (LL 2-1)
        float t = smoothstep(21.0f, 24.0f, hours);
        float lightFloat = lerp(2.0f, 1.0f, t);
        skyLightLevel = static_cast<unsigned char>(std::round(lightFloat));
    }
}