#ifndef PLAYER_H
#define PLAYER_H

class ChunkManager;
class Camera;

enum class GameMode {
    SPECTATOR,
    SURVIVAL
};

class Player {
public:
    // Position
    float x, y, z;

    // Velocity
    float velocityX, velocityY, velocityZ;

    // Player dimensions
    float width;
    float height;
    float eyeHeight;

    // Movement settings
    float walkSpeed;
    float sprintSpeed;
    float jumpStrength;
    float flySpeed;

    // Physics state
    bool isOnGround;
    GameMode gameMode;

    Player(float posX, float posY, float posZ);

    void processInput(float deltaFront, float deltaRight, float deltaUp, bool jump, bool sprint, float deltaTime, Camera& camera);
    void update(float deltaTime, ChunkManager* chunkManager, Camera& camera);
    void setGameMode(GameMode mode);
    GameMode getGameMode() const { return gameMode; }

private:
    void applyPhysics(float deltaTime, ChunkManager* chunkManager);
    bool checkBlockCollision(float x, float y, float z, ChunkManager* chunkManager);
};

#endif
