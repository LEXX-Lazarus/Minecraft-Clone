#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <iostream>
#include <vector>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 26819) // Disable fallthrough warning
#endif
#include "stb_image.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include "Camera.h"
#include "Renderer.h"
#include "Texture.h"
#include "Shader.h"
#include "Window.h"
#include "GUI.h"

// Vertex shader source
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    texCoord = aTexCoord;
}
)";

// Fragment shader source
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 texCoord;

uniform sampler2D ourTexture;

void main()
{
    FragColor = texture(ourTexture, texCoord);
}
)";

// Create identity matrix
void identityMatrix(float* mat) {
    for (int i = 0; i < 16; i++) mat[i] = 0.0f;
    mat[0] = mat[5] = mat[10] = mat[15] = 1.0f;
}

// Create perspective projection matrix
void perspectiveMatrix(float* mat, float fov, float aspect, float near, float far) {
    identityMatrix(mat);
    float tanHalfFov = tan(fov / 2.0f);
    mat[0] = 1.0f / (aspect * tanHalfFov);
    mat[5] = 1.0f / tanHalfFov;
    mat[10] = -(far + near) / (far - near);
    mat[11] = -1.0f;
    mat[14] = -(2.0f * far * near) / (far - near);
    mat[15] = 0.0f;
}

// Create view matrix (lookAt)
void lookAtMatrix(float* mat, float eyeX, float eyeY, float eyeZ,
    float centerX, float centerY, float centerZ,
    float upX, float upY, float upZ) {
    // Calculate forward vector
    float fX = centerX - eyeX;
    float fY = centerY - eyeY;
    float fZ = centerZ - eyeZ;
    float fLen = sqrt(fX * fX + fY * fY + fZ * fZ);
    fX /= fLen; fY /= fLen; fZ /= fLen;

    // Calculate right vector
    float rX = fY * upZ - fZ * upY;
    float rY = fZ * upX - fX * upZ;
    float rZ = fX * upY - fY * upX;
    float rLen = sqrt(rX * rX + rY * rY + rZ * rZ);
    rX /= rLen; rY /= rLen; rZ /= rLen;

    // Calculate up vector
    float uX = rY * fZ - rZ * fY;
    float uY = rZ * fX - rX * fZ;
    float uZ = rX * fY - rY * fX;

    identityMatrix(mat);
    mat[0] = rX; mat[4] = rY; mat[8] = rZ;
    mat[1] = uX; mat[5] = uY; mat[9] = uZ;
    mat[2] = -fX; mat[6] = -fY; mat[10] = -fZ;
    mat[12] = -(rX * eyeX + rY * eyeY + rZ * eyeZ);
    mat[13] = -(uX * eyeX + uY * eyeY + uZ * eyeZ);
    mat[14] = (fX * eyeX + fY * eyeY + fZ * eyeZ);
}

int main(int argc, char* argv[]) {
    Window window("Minecraft Clone", 800, 600);
    if (!window.initialize()) {
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Initialize GUI
    GUI gui;
    gui.initialize();

    // Capture mouse for FPS controls
    SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), true);

    Shader shader(vertexShaderSource, fragmentShaderSource);
    Texture grassTexture("assets/textures/GrassBlock.png");

    // Camera
    Camera camera(0.0f, 1.5f, 3.0f);

    // Renderer
    Renderer renderer;

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    window.togglePause();
                }
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && window.isPaused()) {
                int buttonID;
                if (gui.isButtonClicked(event.button.x, event.button.y,
                    window.getWidth(), window.getHeight(), buttonID)) {
                    if (buttonID == GUI::BUTTON_RESUME) {
                        window.togglePause();
                    }
                    else if (buttonID == GUI::BUTTON_FULLSCREEN) {
                        window.toggleFullscreen();
                    }
                }
            }
            if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                window.handleResize(event.window.data1, event.window.data2);
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION && !window.isPaused()) {
                camera.processMouseMovement(event.motion.xrel, -event.motion.yrel);
            }
        }

        // WASD movement (only when not paused)
        if (!window.isPaused()) {
            const bool* keyState = SDL_GetKeyboardState(nullptr);
            float deltaFront = 0.0f, deltaRight = 0.0f, deltaUp = 0.0f;

            if (keyState[SDL_SCANCODE_W]) deltaFront += 1.0f;
            if (keyState[SDL_SCANCODE_S]) deltaFront -= 1.0f;
            if (keyState[SDL_SCANCODE_D]) deltaRight += 1.0f;
            if (keyState[SDL_SCANCODE_A]) deltaRight -= 1.0f;
            if (keyState[SDL_SCANCODE_SPACE]) deltaUp += 1.0f;
            if (keyState[SDL_SCANCODE_LSHIFT]) deltaUp -= 1.0f;

            camera.processKeyboard(deltaFront, deltaRight, deltaUp);
        }

        glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        grassTexture.bind();

        // Create matrices
        float model[16], view[16], projection[16];

        // Model matrix (identity - cube stays at 0,0,0)
        identityMatrix(model);

        // View matrix (camera looking at front direction)
        lookAtMatrix(view,
            camera.x, camera.y, camera.z,
            camera.x + camera.frontX, camera.y + camera.frontY, camera.z + camera.frontZ,
            0.0f, 1.0f, 0.0f);

        // Projection matrix - USE WINDOW DIMENSIONS
        perspectiveMatrix(projection, 45.0f * 3.14159f / 180.0f,
            (float)window.getWidth() / (float)window.getHeight(), 0.1f, 100.0f);

        // Set uniforms
        unsigned int modelLoc = glGetUniformLocation(shader.getID(), "model");
        unsigned int viewLoc = glGetUniformLocation(shader.getID(), "view");
        unsigned int projLoc = glGetUniformLocation(shader.getID(), "projection");

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection);

        renderer.render();

        // Draw pause menu overlay
        if (window.isPaused()) {
            gui.renderPauseMenu(window.getWidth(), window.getHeight());
        }

        window.swapBuffers();
    }

    SDL_Quit();

    return 0;
}