#include "GUI.h"
#include <iostream>

// Simple 2D vertex shader
const char* guiVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;

out vec4 vertexColor;

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    vertexColor = aColor;
}
)";

// Simple 2D fragment shader
const char* guiFragmentShader = R"(
#version 330 core
out vec4 FragColor;

in vec4 vertexColor;

void main()
{
    FragColor = vertexColor;
}
)";

GUI::GUI() : VAO(0), VBO(0), shaderProgram(0) {
}

GUI::~GUI() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (shaderProgram) glDeleteProgram(shaderProgram);
}

void GUI::initialize() {
    shaderProgram = createGUIShader();
    setupMesh();
}

void GUI::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Color attribute
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

unsigned int GUI::createGUIShader() {
    // Compile vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &guiVertexShader, nullptr);
    glCompileShader(vertexShader);

    int success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "GUI Vertex shader compilation failed:\n" << infoLog << std::endl;
    }

    // Compile fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &guiFragmentShader, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "GUI Fragment shader compilation failed:\n" << infoLog << std::endl;
    }

    // Link shaders
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "GUI Shader program linking failed:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

void GUI::renderPauseMenu(int windowWidth, int windowHeight) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);

    // Dark overlay (fullscreen quad)
    float overlayVertices[] = {
        // Pos          // Color (RGBA)
        -1.0f, -1.0f,   0.0f, 0.0f, 0.0f, 0.7f,
         1.0f, -1.0f,   0.0f, 0.0f, 0.0f, 0.7f,
         1.0f,  1.0f,   0.0f, 0.0f, 0.0f, 0.7f,

        -1.0f, -1.0f,   0.0f, 0.0f, 0.0f, 0.7f,
         1.0f,  1.0f,   0.0f, 0.0f, 0.0f, 0.7f,
        -1.0f,  1.0f,   0.0f, 0.0f, 0.0f, 0.7f
    };

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(overlayVertices), overlayVertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Resume button (green) - top button
    float resumeVertices[] = {
        // Pos          // Color (RGBA)
        -0.3f,  0.15f,  0.2f, 0.7f, 0.2f, 0.9f,
         0.3f,  0.15f,  0.2f, 0.7f, 0.2f, 0.9f,
         0.3f,  0.35f,  0.2f, 0.7f, 0.2f, 0.9f,

        -0.3f,  0.15f,  0.2f, 0.7f, 0.2f, 0.9f,
         0.3f,  0.35f,  0.2f, 0.7f, 0.2f, 0.9f,
        -0.3f,  0.35f,  0.2f, 0.7f, 0.2f, 0.9f
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(resumeVertices), resumeVertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Fullscreen button (blue) - bottom button
    float fullscreenVertices[] = {
        // Pos          // Color (RGBA)
        -0.3f, -0.1f,   0.2f, 0.2f, 0.7f, 0.9f,
         0.3f, -0.1f,   0.2f, 0.2f, 0.7f, 0.9f,
         0.3f,  0.1f,   0.2f, 0.2f, 0.7f, 0.9f,

        -0.3f, -0.1f,   0.2f, 0.2f, 0.7f, 0.9f,
         0.3f,  0.1f,   0.2f, 0.2f, 0.7f, 0.9f,
        -0.3f,  0.1f,   0.2f, 0.2f, 0.7f, 0.9f
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(fullscreenVertices), fullscreenVertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

bool GUI::isButtonClicked(int mouseX, int mouseY, int windowWidth, int windowHeight, int& buttonID) {
    // Convert mouse coordinates to NDC
    float ndcX = (2.0f * mouseX) / windowWidth - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY) / windowHeight;

    // Resume button bounds (NDC coordinates)
    if (ndcX >= -0.3f && ndcX <= 0.3f && ndcY >= 0.15f && ndcY <= 0.35f) {
        buttonID = BUTTON_RESUME;
        return true;
    }

    // Fullscreen button bounds (NDC coordinates)
    if (ndcX >= -0.3f && ndcX <= 0.3f && ndcY >= -0.1f && ndcY <= 0.1f) {
        buttonID = BUTTON_FULLSCREEN;
        return true;
    }

    return false;
}