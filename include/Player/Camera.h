#ifndef CAMERA_H
#define CAMERA_H

class Camera {
public:
    float x, y, z;
    float yaw, pitch;
    float frontX, frontY, frontZ;
    float rightX, rightY, rightZ;
    float speed;
    float sensitivity;

    // Zoom functionality
    float fov;
    float targetFov;
    float zoomSpeed;

    Camera(float posX, float posY, float posZ);

    void processMouseMovement(float xoffset, float yoffset);
    void processZoom(bool zoomIn, float deltaTime);
    float getFOV() const;
    void setPosition(float posX, float posY, float posZ);

private:
    void updateVectors();
};

#endif