#include "Camera.h"
#include <cmath>

Camera::Camera(float posX, float posY, float posZ)
    : x(posX), y(posY), z(posZ),
    yaw(-90.0f), pitch(0.0f),
    frontX(0.0f), frontY(0.0f), frontZ(-1.0f),
    rightX(1.0f), rightY(0.0f), rightZ(0.0f),
    speed(0.05f), sensitivity(0.1f) {
    updateVectors();
}

void Camera::processMouseMovement(float xoffset, float yoffset) {
    yaw -= xoffset * sensitivity;  // NEGATE xoffset
    pitch += yoffset * sensitivity;
    // Constrain pitch
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    updateVectors();
}

void Camera::processKeyboard(float deltaFront, float deltaRight, float deltaUp) {
    x += frontX * deltaFront * speed;
    y += frontY * deltaFront * speed + deltaUp * speed;
    z += frontZ * deltaFront * speed;

    x += rightX * deltaRight * speed;
    y += rightY * deltaRight * speed;
    z += rightZ * deltaRight * speed;
}

void Camera::updateVectors() {
    // Convert to radians
    float yawRad = yaw * 3.14159f / 180.0f;
    float pitchRad = pitch * 3.14159f / 180.0f;

    // Calculate front vector
    frontX = std::cos(yawRad) * std::cos(pitchRad);
    frontY = std::sin(pitchRad);
    frontZ = -std::sin(yawRad) * std::cos(pitchRad);  // NEGATE Z

    // Normalize front
    float length = std::sqrt(frontX * frontX + frontY * frontY + frontZ * frontZ);
    frontX /= length;
    frontY /= length;
    frontZ /= length;

    // Calculate right vector
    rightX = -frontZ;
    rightY = 0.0f;
    rightZ = frontX;

    // Normalize right
    length = std::sqrt(rightX * rightX + rightY * rightY + rightZ * rightZ);
    rightX /= length;
    rightY /= length;
    rightZ /= length;
}