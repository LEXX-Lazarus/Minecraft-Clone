#ifndef CAMERA_H
#define CAMERA_H

class ChunkManager;  // Forward declaration

class Camera {
public:
    // Position
    float x, y, z;

    // Velocity (for survival mode physics)
    float velocityX, velocityY, velocityZ;

    // Rotation
    float yaw, pitch;

    // Direction vectors
    float frontX, frontY, frontZ;
    float rightX, rightY, rightZ;

    // Settings
    float speed;
    float sensitivity;

    // Physics
    bool isOnGround;
    bool isSpectator;

    Camera(float posX, float posY, float posZ);

    void processMouseMovement(float xoffset, float yoffset);
    void processKeyboard(float deltaFront, float deltaRight, float deltaUp, bool jump);
    void update(float deltaTime, ChunkManager* chunkManager);  // NEW: physics update
    void setGameMode(bool spectator);

private:
    void updateVectors();
    void applyPhysics(float deltaTime, ChunkManager* chunkManager);
    void checkCollisions(ChunkManager* chunkManager);
    bool checkBlockCollision(float x, float y, float z, ChunkManager* chunkManager);
};

#endif