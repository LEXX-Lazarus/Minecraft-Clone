#ifndef SKYBOX_H
#define SKYBOX_H

#include <glad/glad.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <vector>

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

    std::vector<Star> stars;
    unsigned int starVAO, starVBO;
    unsigned int starShaderProgram;

    void generateStars();
    void createStarShader();
};

#endif