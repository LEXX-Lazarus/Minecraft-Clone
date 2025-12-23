#include "Rendering/Lighting.h"
#include <glad/glad.h>
#include <cmath>

Lighting::Lighting()
    : sunColor(1.0f, 0.95f, 0.8f),  // Slightly warm sunlight
    ambientStrength(0.4f)          // 40% ambient light
{
    // Sun at 2:00 PM position
    // West = -X, Up = +Y, North = +Z
    // 2 PM = sun in the west, slightly high in sky
    float angle = 60.0f * (3.14159f / 180.0f);  // 60 degrees up from horizon

    sunDirection = glm::vec3(
        0.7f,                    // Coming from the west (negative X)
        0.5f,                    // Angled down from above
        0.0f                     // Centered north-south
    );
    sunDirection = glm::normalize(sunDirection);
}

void Lighting::applyToShader(unsigned int shaderID) {
    glUseProgram(shaderID);

    // Send sun direction
    int sunDirLoc = glGetUniformLocation(shaderID, "sunDirection");
    glUniform3f(sunDirLoc, sunDirection.x, sunDirection.y, sunDirection.z);

    // Send sun color
    int sunColorLoc = glGetUniformLocation(shaderID, "sunColor");
    glUniform3f(sunColorLoc, sunColor.r, sunColor.g, sunColor.b);

    // Send ambient strength
    int ambientLoc = glGetUniformLocation(shaderID, "ambientStrength");
    glUniform1f(ambientLoc, ambientStrength);
}

void Lighting::setTimeOfDay(float time) {
    // Optional: For future day/night cycle
    // time: 0.0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset, 1.0 = midnight
    float angle = time * 3.14159f * 2.0f;

    sunDirection = glm::vec3(
        std::cos(angle),
        std::sin(angle),
        0.0f
    );
    sunDirection = glm::normalize(sunDirection);
}