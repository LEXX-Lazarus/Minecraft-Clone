#include "Player.h"
#include "Camera.h"
#include "ChunkManager.h"
#include <cmath>
#include <algorithm>
#include <iostream>

Player::Player(float posX, float posY, float posZ)
    : x(posX), y(posY), z(posZ),
    velocityX(0.0f), velocityY(0.0f), velocityZ(0.0f),
    width(0.6f), height(1.8f), eyeHeight(1.0f),
    walkSpeed(4.317f), sprintSpeed(5.612f), jumpStrength(10.0f),
    isOnGround(false), gameMode(GameMode::SPECTATOR) {
}

void Player::setGameMode(GameMode mode) {
    gameMode = mode;
    if (mode == GameMode::SPECTATOR) {
        velocityX = velocityY = velocityZ = 0.0f;
        isOnGround = false;
    }
}

void Player::processInput(float deltaFront, float deltaRight, float deltaUp, bool jump, bool sprint, Camera& camera) {
    if (gameMode == GameMode::SPECTATOR) {
        float speed = sprint ? camera.speed * 2.0f : camera.speed;

        x += camera.frontX * deltaFront * speed;
        y += camera.frontY * deltaFront * speed + deltaUp * speed;
        z += camera.frontZ * deltaFront * speed;

        x += camera.rightX * deltaRight * speed;
        y += camera.rightY * deltaRight * speed;
        z += camera.rightZ * deltaRight * speed;

    }
    else {
        float moveX = camera.frontX * deltaFront + camera.rightX * deltaRight;
        float moveZ = camera.frontZ * deltaFront + camera.rightZ * deltaRight;

        float length = std::sqrt(moveX * moveX + moveZ * moveZ);
        if (length > 0.01f) {
            moveX /= length;
            moveZ /= length;
        }

        float currentSpeed = sprint ? sprintSpeed : walkSpeed;
        velocityX = moveX * currentSpeed;
        velocityZ = moveZ * currentSpeed;

        if (jump && isOnGround) {
            velocityY = jumpStrength;
            isOnGround = false;
        }
    }
}

void Player::update(float deltaTime, ChunkManager* chunkManager, Camera& camera) {
    if (gameMode == GameMode::SURVIVAL) {
        applyPhysics(deltaTime, chunkManager);
    }

    camera.x = x;
    camera.y = y + eyeHeight;
    camera.z = z;
}

void Player::applyPhysics(float deltaTime, ChunkManager* chunkManager) {
    const float GRAVITY = -32.0f;
    const float MAX_FALL_SPEED = -78.4f;
    const float DRAG = 0.91f;

    // Apply gravity
    if (!isOnGround) {
        velocityY += GRAVITY * deltaTime;
        if (velocityY < MAX_FALL_SPEED) {
            velocityY = MAX_FALL_SPEED;
        }
    }
    else {
        velocityY = -0.08f;
    }

    velocityX *= DRAG;
    velocityZ *= DRAG;

    // Move each axis separately with collision detection
    // X-axis movement
    float moveX = velocityX * deltaTime;
    if (moveX != 0.0f) {
        if (!checkBlockCollision(x + moveX, y, z, chunkManager)) {
            x += moveX;
        }
        else {
            velocityX = 0.0f;
        }
    }

    // Y-axis movement
    float moveY = velocityY * deltaTime;
    if (moveY != 0.0f) {
        if (!checkBlockCollision(x, y + moveY, z, chunkManager)) {
            y += moveY;
            isOnGround = false;
        }
        else {
            if (velocityY < 0.0f) {
                isOnGround = true;
            }
            velocityY = 0.0f;
        }
    }

    // Z-axis movement
    float moveZ = velocityZ * deltaTime;
    if (moveZ != 0.0f) {
        if (!checkBlockCollision(x, y, z + moveZ, chunkManager)) {
            z += moveZ;
        }
        else {
            velocityZ = 0.0f;
        }
    }
}

bool Player::checkBlockCollision(float px, float py, float pz, ChunkManager* chunkManager) {
    if (!chunkManager) return false;

    float halfWidth = width / 2.0f;

    // Debug: Print player position once
    static bool debugPrinted = false;
    if (!debugPrinted && gameMode == GameMode::SURVIVAL) {
        std::cout << "Player center at: (" << px << ", " << py << ", " << pz << ")" << std::endl;
        std::cout << "Bounding box extends: " << halfWidth << " in each horizontal direction" << std::endl;
        debugPrinted = true;
    }

    // Check multiple heights
    std::vector<float> checkHeights = { 0.01f, 0.5f, 1.0f, 1.5f, 1.79f };

    for (float dy : checkHeights) {
        // Check 8 points around the perimeter + center
        std::vector<std::pair<float, float>> checkPoints = {
            {-halfWidth, -halfWidth},
            {halfWidth, -halfWidth},
            {-halfWidth, halfWidth},
            {halfWidth, halfWidth},
            {0.0f, -halfWidth},
            {0.0f, halfWidth},
            {-halfWidth, 0.0f},
            {halfWidth, 0.0f},
            {0.0f, 0.0f}
        };

        for (auto [dx, dz] : checkPoints) {
            float checkX = px + dx;
            float checkY = py + dy;
            float checkZ = pz + dz;

            // FIXED: Blocks are rendered centered at integer coordinates
            // A block at position (0, 0, 0) spans from (-0.5, -0.5, -0.5) to (0.5, 0.5, 0.5)
            // So we need to round to nearest integer, not floor
            int blockX = static_cast<int>(std::round(checkX));
            int blockY = static_cast<int>(std::floor(checkY));  // Y uses floor (feet on ground)
            int blockZ = static_cast<int>(std::round(-checkZ)); // Round, then negate for chunk system

            Block* block = chunkManager->getBlockAt(blockX, blockY, blockZ);
            if (block) {
                bool isSolid = !block->isAir();
                delete block;
                if (isSolid) {
                    return true;
                }
            }
        }
    }

    return false;
}