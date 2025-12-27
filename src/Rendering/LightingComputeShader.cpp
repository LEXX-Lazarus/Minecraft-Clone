#include "Rendering/LightingComputeShader.h"
#include <iostream>

LightingComputeShader::LightingComputeShader() {}

LightingComputeShader::~LightingComputeShader() {
    if (shaderProgram) glDeleteProgram(shaderProgram);
}

void LightingComputeShader::initialize() {
    const char* computeSource = R"(
    #version 430

    layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
    layout(r8ui, binding = 0) uniform uimage3D lightMap;
    uniform int chunkSizeX;
    uniform int chunkSizeY;
    uniform int chunkSizeZ;

    void main() {
        ivec3 pos = ivec3(gl_GlobalInvocationID);
        if (pos.x >= chunkSizeX || pos.y >= chunkSizeY || pos.z >= chunkSizeZ) return;

        uint currentLight = imageLoad(lightMap, pos).r;

        // Simple downward propagation example
        if (pos.y > 0) {
            ivec3 below = pos + ivec3(0, -1, 0);
            uint belowLight = imageLoad(lightMap, below).r;
            uint newLight = max(belowLight > 0 ? belowLight - 1u : 0u, currentLight);
            imageStore(lightMap, pos, uvec4(newLight, 0, 0, 0));
        }
    }
    )";

    shaderProgram = createComputeShader(computeSource);
}

GLuint LightingComputeShader::createComputeShader(const char* source) {
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Compute shader compilation error: " << log << std::endl;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glDeleteShader(shader);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        std::cerr << "Compute shader linking error: " << log << std::endl;
    }

    return program;
}

void LightingComputeShader::run(GLuint lightTexture, int chunkSizeX, int chunkSizeY, int chunkSizeZ) {
    glUseProgram(shaderProgram);
    glBindImageTexture(0, lightTexture, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8UI);

    glUniform1i(glGetUniformLocation(shaderProgram, "chunkSizeX"), chunkSizeX);
    glUniform1i(glGetUniformLocation(shaderProgram, "chunkSizeY"), chunkSizeY);
    glUniform1i(glGetUniformLocation(shaderProgram, "chunkSizeZ"), chunkSizeZ);

    int groupsX = (chunkSizeX + 7) / 8;
    int groupsY = (chunkSizeY + 7) / 8;
    int groupsZ = (chunkSizeZ + 7) / 8;

    glDispatchCompute(groupsX, groupsY, groupsZ);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
