#include "Camera.h"
#include "ChunkManager.h"
#include <cmath>
#include <algorithm>

Camera::Camera(float posX, float posY, float posZ)
    : x(posX), y(posY), z(posZ),
    velocityX(0.0f), velocityY(0.0f), velocityZ(0.0f),
    yaw(90.0f), pitch(0.0f),
    frontX(0.0f), frontY(0.0f), frontZ(-1.0f),
    rightX(1.0f), rightY(0.0f), rightZ(0.0f),
    speed(0.05f), sensitivity(0.1f),
    isOnGround(false), isSpectator(true) {
    updateVectors();
}

void Camera::setGameMode(bool spectator) {
    isSpectator = spectator;
    if (spectator) {
        // Reset physics when entering spectator
        velocityX = velocityY = velocityZ = 0.0f;
    }
}

void Camera::processMouseMovement(float xoffset, float yoffset) {
    yaw -= xoffset * sensitivity;
    pitch += yoffset * sensitivity;

    // Constrain pitch
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    updateVectors();
}

void Camera::processKeyboard(float deltaFront, float deltaRight, float deltaUp, bool jump) {
    if (isSpectator) {
        // SPECTATOR MODE: Free flight, no physics
        x += frontX * deltaFront * speed;
        y += frontY * deltaFront * speed + deltaUp * speed;
        z += frontZ * deltaFront * speed;

        x += rightX * deltaRight * speed;
        y += rightY * deltaRight * speed;
        z += rightZ * deltaRight * speed;
    }
    else {
        // SURVIVAL MODE: Ground-based movement
        // Movement is horizontal only (ignore pitch for WASD)
        float moveX = frontX * deltaFront + rightX * deltaRight;
        float moveZ = frontZ * deltaFront + rightZ * deltaRight;

        // Normalize diagonal movement
        float length = std::sqrt(moveX * moveX + moveZ * moveZ);
        if (length > 0.01f) {
            moveX /= length;
            moveZ /= length;
        }

        // Apply movement (no vertical component from looking up/down)
        velocityX = moveX * speed * 3.0f;  // Faster ground speed
        velocityZ = moveZ * speed * 3.0f;

        // Jump
        if (jump && isOnGround) {
            velocityY = 0.2f;  // Jump strength
            isOnGround = false;
        }
    }
}

void Camera::update(float deltaTime, ChunkManager* chunkManager) {
    if (!isSpectator) {
        applyPhysics(deltaTime, chunkManager);
    }
}

void Camera::applyPhysics(float deltaTime, ChunkManager* chunkManager) {
    const float GRAVITY = -0.5f;
    const float MAX_FALL_SPEED = -2.0f;
    const float FRICTION = 0.8f;

    // Apply gravity
    if (!isOnGround) {
        velocityY += GRAVITY * deltaTime;
        if (velocityY < MAX_FALL_SPEED) velocityY = MAX_FALL_SPEED;
    }

    // Apply friction to horizontal movement
    velocityX *= FRICTION;
    velocityZ *= FRICTION;

    // Try to move
    float newX = x + velocityX;
    float newY = y + velocityY;
    float newZ = z + velocityZ;

    // Check collisions
    bool collidedX = checkBlockCollision(newX, y, z, chunkManager);
    bool collidedY = checkBlockCollision(x, newY, z, chunkManager);
    bool collidedZ = checkBlockCollision(x, y, newZ, chunkManager);

    // Apply movement if no collision
    if (!collidedX) x = newX;
    else velocityX = 0.0f;

    if (!collidedY) {
        y = newY;
        isOnGround = false;
    }
    else {
        if (velocityY < 0.0f) {
            isOnGround = true;
        }
        velocityY = 0.0f;
    }

    if (!collidedZ) z = newZ;
    else velocityZ = 0.0f;
}

bool Camera::checkBlockCollision(float x, float y, float z, ChunkManager* chunkManager) {
    if (!chunkManager) return false;

    const float PLAYER_WIDTH = 0.3f;
    const float PLAYER_HEIGHT = 1.8f;

    // Check multiple points around player bounding box
    for (float dy = 0.0f; dy <= PLAYER_HEIGHT; dy += PLAYER_HEIGHT / 2.0f) {
        for (float dx = -PLAYER_WIDTH; dx <= PLAYER_WIDTH; dx += PLAYER_WIDTH * 2.0f) {
            for (float dz = -PLAYER_WIDTH; dz <= PLAYER_WIDTH; dz += PLAYER_WIDTH * 2.0f) {
                // Convert to block coordinates
                int blockX = static_cast<int>(std::floor(x + dx));
                int blockY = static_cast<int>(std::floor(y + dy));
                int blockZ = static_cast<int>(std::floor(-(z + dz)));  // Negate Z

                // Get block at position
                Block* block = chunkManager->getBlockAt(blockX, blockY, blockZ);
                if (block) {
                    bool isSolid = !block->isAir();
                    delete block;
                    if (isSolid) {
                        return true;  // Collision!
                    }
                }
            }
        }
    }

    return false;
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