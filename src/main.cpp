#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <thread>

#define STB_IMAGE_IMPLEMENTATION
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 26819)
#endif
#include "stb_image.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "Player/Camera.h"
#include "Player/Player.h"
#include "Player/BlockInteraction.h"
#include "Rendering/Renderer.h"
#include "Rendering/Texture.h"
#include "Rendering/Shader.h"
#include "Rendering/TextureAtlas.h"
#include "Window.h"
#include "GUI/PauseMenu.h"
#include "GUI/DebugOverlay.h"
#include "GUI/Crosshair.h"
#include "GUI/BlockOutline.h"
#include "World/Chunk.h"
#include "World/TerrainGenerator.h"
#include "World/ChunkManager.h"

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

void identityMatrix(float* mat) {
    for (int i = 0; i < 16; i++) mat[i] = 0.0f;
    mat[0] = mat[5] = mat[10] = mat[15] = 1.0f;
}

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

void lookAtMatrix(float* mat, float eyeX, float eyeY, float eyeZ,
    float centerX, float centerY, float centerZ,
    float upX, float upY, float upZ) {
    float fX = centerX - eyeX;
    float fY = centerY - eyeY;
    float fZ = centerZ - eyeZ;
    float fLen = sqrt(fX * fX + fY * fY + fZ * fZ);
    fX /= fLen; fY /= fLen; fZ /= fLen;

    float rX = fY * upZ - fZ * upY;
    float rY = fZ * upX - fX * upZ;
    float rZ = fX * upY - fY * upX;
    float rLen = sqrt(rX * rX + rY * rY + rZ * rZ);
    rX /= rLen; rY /= rLen; rZ /= rLen;

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

    PauseMenu pauseMenu;
    pauseMenu.initialize();

    Crosshair crosshair;
    crosshair.initialize();

    BlockOutline blockOutline;
    blockOutline.initialize();

    DebugOverlay debugOverlay;
    debugOverlay.initialize();

    SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), true);

    Shader shader(vertexShaderSource, fragmentShaderSource);

    TextureAtlas atlas(64, 48);
    if (!atlas.buildAtlas("assets/textures/blocks/")) {
        std::cerr << "Failed to build texture atlas!" << std::endl;
        return -1;
    }

    float spawnX = 0.0f;
    float spawnZ = 0.0f;
    float spawnY = 270.0f;

    ChunkManager chunkManager(12);
    chunkManager.setTextureAtlas(&atlas);
    chunkManager.setVerticalRenderDistance(6);

    int blockX = static_cast<int>(std::round(spawnX));
    int blockZ = static_cast<int>(std::round(-spawnZ));
    int highestSolidY = -1;

    for (int checkY = 280; checkY < 300; ++checkY) {
        Block* block = chunkManager.getBlockAt(blockX, checkY, blockZ);
        if (block) {
            if (!block->isAir()) {
                highestSolidY = checkY;
            }
            delete block;
        }
    }

    if (highestSolidY >= 0) {
        spawnY = static_cast<float>(highestSolidY) + 1.0f;
    }
    else {
        spawnY = 280.0f;
    }

    Player player(spawnX, spawnY, spawnZ);
    Camera camera(spawnX, spawnY, spawnZ);
    player.setGameMode(GameMode::SURVIVAL);

    BlockInteraction blockInteraction;
    BlockType selectedBlock = Blocks::STONE;

    bool running = true;
    SDL_Event event;

    auto lastTime = std::chrono::high_resolution_clock::now();
    auto lastFrameTime = lastTime;
    int fpsFrameCount = 0;
    float fps = 0.0f;

    // Pre-cache keyboard state pointer to avoid repeated calls
    const bool* keyState = nullptr;

    while (running) {
        auto currentFrameTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
        lastFrameTime = currentFrameTime;

        // **FIX FOR LAG SPIKE**: Cap deltaTime to prevent huge jumps when resuming
        // Max 100ms (0.1s) to handle pauses gracefully
        if (deltaTime > 0.1f) {
            deltaTime = 0.1f;
        }

        fpsFrameCount++;
        auto currentTime = std::chrono::high_resolution_clock::now();
        float fpsTime = std::chrono::duration<float>(currentTime - lastTime).count();
        if (fpsTime >= 1.0f) {
            fps = fpsFrameCount / fpsTime;
            fpsFrameCount = 0;
            lastTime = currentTime;
        }

        // Event handling
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }

            if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) {
                case SDLK_ESCAPE:
                    window.togglePause();
                    // **FIX**: Reset frame time when pausing to prevent deltaTime spike
                    lastFrameTime = std::chrono::high_resolution_clock::now();
                    break;

                case SDLK_F3:
                    debugOverlay.toggle();
                    break;

                case SDLK_F1: {
                    GameMode newMode = (player.getGameMode() == GameMode::SPECTATOR)
                        ? GameMode::SURVIVAL
                        : GameMode::SPECTATOR;
                    player.setGameMode(newMode);
                    std::cout << "Switched to "
                        << (newMode == GameMode::SPECTATOR ? "SPECTATOR" : "SURVIVAL")
                        << " mode" << std::endl;
                    break;
                }

                case SDLK_1:
                    selectedBlock = Blocks::STONE;
                    std::cout << "Selected: STONE" << std::endl;
                    break;
                case SDLK_2:
                    selectedBlock = Blocks::DIRT;
                    std::cout << "Selected: DIRT" << std::endl;
                    break;
                case SDLK_3:
                    selectedBlock = Blocks::GRASS;
                    std::cout << "Selected: GRASS" << std::endl;
                    break;
                case SDLK_4:
                    selectedBlock = Blocks::SAND;
                    std::cout << "Selected: SAND" << std::endl;
                    break;
                case SDLK_5:
                    selectedBlock = Blocks::OAK_LOG;
                    std::cout << "Selected: OAK_LOG" << std::endl;
                    break;
                case SDLK_6:
                    selectedBlock = Blocks::OAK_LEAVES;
                    std::cout << "Selected: OAK_LEAVES" << std::endl;
                    break;
                case SDLK_7:
                    selectedBlock = Blocks::BLOCK_OF_WHITE_LIGHT;
                    std::cout << "Selected: BLOCK_OF_WHITE_LIGHT" << std::endl;
                    break;
                case SDLK_8:
                    selectedBlock = Blocks::BLOCK_OF_RED_LIGHT;
                    std::cout << "Selected: BLOCK_OF_RED_LIGHT" << std::endl;
                    break;
                case SDLK_9:
                    selectedBlock = Blocks::BLOCK_OF_GREEN_LIGHT;
                    std::cout << "Selected: BLOCK_OF_GREEN_LIGHT" << std::endl;
                    break;
                case SDLK_0:
                    selectedBlock = Blocks::BLOCK_OF_BLUE_LIGHT;
                    std::cout << "Selected: BLOCK_OF_BLUE_LIGHT" << std::endl;
                    break;
                }
            }

            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (window.isPaused()) {
                    int buttonID;
                    if (pauseMenu.isButtonClicked(event.button.x, event.button.y,
                        window.getWidth(), window.getHeight(), buttonID)) {
                        if (buttonID == PauseMenu::BUTTON_RESUME) {
                            window.togglePause();
                            // **FIX**: Reset frame time when unpausing
                            lastFrameTime = std::chrono::high_resolution_clock::now();
                        }
                        else if (buttonID == PauseMenu::BUTTON_FULLSCREEN) {
                            window.toggleFullscreen();
                        }
                    }
                }
                else {
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        blockInteraction.breakBlock(camera, &chunkManager);
                    }
                    else if (event.button.button == SDL_BUTTON_RIGHT) {
                        blockInteraction.placeBlock(camera, &chunkManager, selectedBlock);
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

        // Game logic updates (only when not paused)
        if (!window.isPaused()) {
            // Cache keyboard state once per frame
            keyState = SDL_GetKeyboardState(nullptr);

            float deltaFront = 0.0f, deltaRight = 0.0f, deltaUp = 0.0f;
            bool jump = false;
            bool sprint = keyState[SDL_SCANCODE_LSHIFT] || keyState[SDL_SCANCODE_RSHIFT];
            bool zoom = keyState[SDL_SCANCODE_Z];

            if (keyState[SDL_SCANCODE_W]) deltaFront += 1.0f;
            if (keyState[SDL_SCANCODE_S]) deltaFront -= 1.0f;
            if (keyState[SDL_SCANCODE_D]) deltaRight += 1.0f;
            if (keyState[SDL_SCANCODE_A]) deltaRight -= 1.0f;

            if (player.getGameMode() == GameMode::SPECTATOR) {
                if (keyState[SDL_SCANCODE_SPACE]) deltaUp += 1.0f;
                if (keyState[SDL_SCANCODE_LCTRL] || keyState[SDL_SCANCODE_RCTRL]) deltaUp -= 1.0f;
            }
            else {
                if (keyState[SDL_SCANCODE_SPACE]) jump = true;
            }

            player.processInput(deltaFront, deltaRight, deltaUp, jump, sprint, camera);
            camera.processZoom(zoom, deltaTime);
            player.update(deltaTime, &chunkManager, camera);
            chunkManager.update(player.x, player.y, player.z);
        }

        // Rendering
        glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        float model[16], view[16], projection[16];
        identityMatrix(model);

        lookAtMatrix(view,
            camera.x, camera.y, camera.z,
            camera.x + camera.frontX, camera.y + camera.frontY, camera.z + camera.frontZ,
            0.0f, 1.0f, 0.0f);

        float currentFOV = camera.getFOV() * 3.14159f / 180.0f;
        perspectiveMatrix(projection, currentFOV,
            (float)window.getWidth() / (float)window.getHeight(), 0.1f, 1000.0f);

        unsigned int modelLoc = glGetUniformLocation(shader.getID(), "model");
        unsigned int viewLoc = glGetUniformLocation(shader.getID(), "view");
        unsigned int projLoc = glGetUniformLocation(shader.getID(), "projection");

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection);

        // Bind texture atlas once for all block types
        atlas.bind();

        // Render all block types
        chunkManager.renderType(Blocks::GRASS);
        chunkManager.renderType(Blocks::DIRT);
        chunkManager.renderType(Blocks::STONE);
        chunkManager.renderType(Blocks::SAND);
        chunkManager.renderType(Blocks::OAK_LOG);
        chunkManager.renderType(Blocks::OAK_LEAVES);
        chunkManager.renderType(Blocks::BLOCK_OF_WHITE_LIGHT);
        chunkManager.renderType(Blocks::BLOCK_OF_RED_LIGHT);
        chunkManager.renderType(Blocks::BLOCK_OF_GREEN_LIGHT);
        chunkManager.renderType(Blocks::BLOCK_OF_BLUE_LIGHT);

        // Render UI elements
        if (!window.isPaused()) {
            blockOutline.render(camera, &chunkManager, view, projection);
            crosshair.render(window.getWidth(), window.getHeight());
        }

        debugOverlay.render(window.getWidth(), window.getHeight(),
            camera.x, camera.y, camera.z,
            camera.yaw, fps);

        if (window.isPaused()) {
            pauseMenu.render(window.getWidth(), window.getHeight());
        }

        window.swapBuffers();
    }

    SDL_Quit();
    return 0;
}