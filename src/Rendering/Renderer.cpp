#include "Rendering/Renderer.h"
#include <glad/glad.h>

Renderer::Renderer() {
    setupMesh();
}

Renderer::~Renderer() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void Renderer::setupMesh() {
    // Cube vertices - positioned at world origin (0, 0, 0)
    static const float vertices[] = {
        // Positions          // Texture Coords
        // Top face (y = 0.5) - FIXED WINDING ORDER
        -0.5f,  0.5f, -0.5f,   0.25f, 1.0f,
        -0.5f,  0.5f,  0.5f,   0.25f, 0.666f,
        0.5f,  0.5f,  0.5f,   0.5f,  0.666f,
        0.5f,  0.5f, -0.5f,   0.5f,  1.0f,

        // Bottom face (y = -0.5) - FIXED WINDING ORDER
        -0.5f, -0.5f,  0.5f,   0.25f, 0.333f,
        -0.5f, -0.5f, -0.5f,   0.25f, 0.0f,
        0.5f, -0.5f, -0.5f,   0.5f,  0.0f,
        0.5f, -0.5f,  0.5f,   0.5f,  0.333f,

        // South face (z = 0.5)
        -0.5f, -0.5f,  0.5f,   0.25f, 0.333f,
        0.5f, -0.5f,  0.5f,   0.5f,  0.333f,
        0.5f,  0.5f,  0.5f,   0.5f,  0.666f,
        -0.5f,  0.5f,  0.5f,   0.25f, 0.666f,

        // North face (z = -0.5)
        0.5f, -0.5f, -0.5f,   0.75f, 0.333f,
        -0.5f, -0.5f, -0.5f,   1.0f,  0.333f,
        -0.5f,  0.5f, -0.5f,   1.0f,  0.666f,
        0.5f,  0.5f, -0.5f,   0.75f, 0.666f,

        // East face (x = 0.5)
        0.5f, -0.5f,  0.5f,   0.5f,  0.333f,
        0.5f, -0.5f, -0.5f,   0.75f, 0.333f,
        0.5f,  0.5f, -0.5f,   0.75f, 0.666f,
        0.5f,  0.5f,  0.5f,   0.5f,  0.666f,

        // West face (x = -0.5)
        -0.5f, -0.5f, -0.5f,   0.0f,  0.333f,
        -0.5f, -0.5f,  0.5f,   0.25f, 0.333f,
        -0.5f,  0.5f,  0.5f,   0.25f, 0.666f,
        -0.5f,  0.5f, -0.5f,   0.0f,  0.666f,
    };

    static const unsigned int indices[] = {
        0, 1, 2, 2, 3, 0,       // Top
        4, 5, 6, 6, 7, 4,       // Bottom
        8, 9, 10, 10, 11, 8,    // South
        12, 13, 14, 14, 15, 12, // North
        16, 17, 18, 18, 19, 16, // East
        20, 21, 22, 22, 23, 20  // West
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0); // Unbind
}

void Renderer::render() {
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0); // Unbind after rendering
}