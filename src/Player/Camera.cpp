#include "Player/Camera.h"
#include <cmath>

Camera::Camera(float posX, float posY, float posZ)
    : x(posX), y(posY), z(posZ),
    yaw(90.0f), pitch(0.0f),
    frontX(0.0f), frontY(0.0f), frontZ(-1.0f),
    rightX(1.0f), rightY(0.0f), rightZ(0.0f),
    speed(0.55f), sensitivity(0.1f),
    fov(70.0f), targetFov(70.0f), zoomSpeed(5.0f) {
    updateVectors();
}

void Camera::processMouseMovement(float xoffset, float yoffset) {
    yaw -= xoffset * sensitivity;
    pitch += yoffset * sensitivity;

    // Constrain pitch
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    updateVectors();
}

void Camera::processZoom(bool zoomIn, float deltaTime) {
    // Smooth zoom transition
    if (zoomIn) {
        targetFov = 30.0f; // Zoomed in FOV (adjust this value for more/less zoom)
    }
    else {
        targetFov = 70.0f; // Normal FOV
    }

    // Smoothly interpolate current FOV towards target
    float diff = targetFov - fov;
    fov += diff * zoomSpeed * deltaTime;

    // Clamp to prevent overshooting
    if (std::abs(diff) < 0.1f) {
        fov = targetFov;
    }
}

float Camera::getFOV() const {
    return fov;
}

void Camera::setPosition(float posX, float posY, float posZ) {
    x = posX;
    y = posY;
    z = posZ;
}

void Camera::updateVectors() {
    float yawRad = yaw * 3.14159f / 180.0f;
    float pitchRad = pitch * 3.14159f / 180.0f;

    frontX = std::cos(yawRad) * std::cos(pitchRad);
    frontY = std::sin(pitchRad);
    frontZ = -std::sin(yawRad) * std::cos(pitchRad);

    float length = std::sqrt(frontX * frontX + frontY * frontY + frontZ * frontZ);
    frontX /= length;
    frontY /= length;
    frontZ /= length;

    rightX = -frontZ;
    rightY = 0.0f;
    rightZ = frontX;

    length = std::sqrt(rightX * rightX + rightY * rightY + rightZ * rightZ);
    rightX /= length;
    rightY /= length;
    rightZ /= length;
}