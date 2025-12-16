#ifndef CAMERA_H
#define CAMERA_H

class Camera {
public:
    // Position
    float x, y, z;

    // Rotation
    float yaw, pitch;

    // Direction vectors
    float frontX, frontY, frontZ;
    float rightX, rightY, rightZ;

    // Settings
    float speed;
    float sensitivity;

    Camera(float posX, float posY, float posZ);

    void processMouseMovement(float xoffset, float yoffset);
    void processKeyboard(float deltaFront, float deltaRight, float deltaUp);

private:
    void updateVectors();
};

#endif