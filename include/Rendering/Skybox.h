#ifndef SKYBOX_H
#define SKYBOX_H

#include <glad/glad.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <vector>
#include <string>

class Skybox {
public:
    Skybox();
    ~Skybox();

    void initialize();
    void render(const float* viewMatrix, const float* projectionMatrix, float timeOfDay);

private:
    struct Star {
        glm::vec3 position;
        float brightness;
    };

    struct CelestialBody {
        unsigned int VAO, VBO;
        unsigned int shaderProgram;
        unsigned int textureID; // ID for the texture (Sun.png / FullMoon.png)
        float size;
        float distance;
    };

    std::vector<Star> stars;
    unsigned int starVAO, starVBO;
    unsigned int starShaderProgram;

    CelestialBody sun;
    CelestialBody moon;

    void generateStars();
    void createStarShader();
    void createSunMoonShader();

    // Helper to load textures using stb_image
    unsigned int loadTexture(const char* path);

    glm::vec3 getCelestialPosition(float timeOfDay, bool isSun);
    void renderCelestial(const float* viewMatrix, const float* projectionMatrix, float timeOfDay);
};

#endif