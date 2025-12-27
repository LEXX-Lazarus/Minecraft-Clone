#pragma once
#include <glad/glad.h>
#include <vector>

class LightingComputeShader {
public:
    LightingComputeShader();
    ~LightingComputeShader();

    void initialize();
    void run(GLuint lightTexture, int chunkSizeX, int chunkSizeY, int chunkSizeZ);

private:
    GLuint shaderProgram = 0;
    GLuint createComputeShader(const char* source);
};
