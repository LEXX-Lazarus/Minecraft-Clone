#ifndef CROSSHAIR_H
#define CROSSHAIR_H

#include <glad/glad.h>

class Crosshair {
public:
    Crosshair();
    ~Crosshair();

    void initialize();
    void render(int windowWidth, int windowHeight);

private:
    unsigned int VAO, VBO;
    unsigned int shaderProgram;
    unsigned int centerPixelFBO;  // Framebuffer to sample center pixel
    unsigned int centerPixelTexture;

    float crosshairColor[3];  // RGB color

    void setupMesh();
    unsigned int createShader();
    void sampleCenterPixel(int windowWidth, int windowHeight);
    void calculateAdaptiveColor(float r, float g, float b);
};

#endif