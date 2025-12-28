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
#include "Rendering/Skybox.h"
#include "Rendering/Lighting.h"
#include "Window.h"
#include "GUI/PauseMenu.h"
#include "GUI/DebugOverlay.h"
#include "GUI/Crosshair.h"
#include "GUI/BlockOutline.h"
#include "GUI/HUD.h"
#include "Chunk.h"
#include "TerrainGenerator.h"
#include "ChunkManager.h"

// GPU-OPTIMIZED: Shader receives global light level and scales in real-time
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in float aLightLevel;  // This is the MAX light (0-1), calculated once

out vec2 texCoord;
out vec3 normal;
out float lightLevel;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    texCoord = aTexCoord;
    normal = aNormal;
    lightLevel = aLightLevel;  // Pass max light to fragment shader
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

// GPU-OPTIMIZED: Fragment shader scales light based on global level
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 texCoord;
in vec3 normal;
in float lightLevel;  // Max light level (0-1) from vertex

uniform sampler2D ourTexture;
uniform float globalSkyLightLevel;  // Current sky light (0-15)

void main()
{
    vec4 texColor = texture(ourTexture, texCoord);
    vec3 norm = normalize(normal);
    
    // GPU MAGIC: Scale the max light by current global level
    // This happens on GPU for ALL chunks simultaneously!
    float actualLight = lightLevel * (globalSkyLightLevel / 15.0);
    
    // Minecraft-style brightness curve
    float brightness = pow(actualLight, 2.2) * 0.95 + 0.05;
    
    // Ambient occlusion
    float ao = 1.0;
    if (norm.y > 0.9) ao = 1.0;
    else if (norm.y < -0.9) ao = 0.5;
    else ao = 0.8;
    
    vec3 lighting = vec3(brightness * ao);
    vec3 result = lighting * texColor.rgb;
    
    FragColor = vec4(result, texColor.a);
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

    Lighting lighting;

    HUD hud;
    hud.initialize();

    SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), true);

    Shader shader(vertexShaderSource, fragmentShaderSource);
    Texture grassBlockTexture("assets/textures/blocks/GrassBlock.png");
    Texture dirtBlockTexture("assets/textures/blocks/DirtBlock.png");
    Texture stoneBlockTexture("assets/textures/blocks/StoneBlock.png");
    Texture sandBlockTexture("assets/textures/blocks/SandBlock.png");
	Texture blockOfPureWhiteLightTexture("assets/textures/blocks/BlockOfPureWhiteLight.png");
	Texture blockOfPureRedLightTexture("assets/textures/blocks/BlockOfPureRedLight.png");
	Texture blockOfPureGreenLightTexture("assets/textures/blocks/BlockOfPureGreenLight.png");
	Texture blockOfPureBlueLightTexture("assets/textures/blocks/BlockOfPureBlueLight.png");

    float spawnX = 0.0f;
    float spawnZ = 0.0f;
    float spawnY = 120.0f;

    ChunkManager chunkManager(12, "world1");

    int blockX = static_cast<int>(std::round(spawnX));
    int blockZ = static_cast<int>(std::round(-spawnZ));

    int highestSolidY = -1;

    for (int checkY = 120; checkY < 200; ++checkY) {
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
        spawnY = 120.0f;
    }

    Player player(spawnX, spawnY, spawnZ);
    Camera camera(spawnX, spawnY, spawnZ);
    player.setGameMode(GameMode::SURVIVAL);

    BlockInteraction blockInteraction;
    BlockType selectedBlock = BlockType::STONE;

    Skybox skybox;
    skybox.initialize();

    bool running = true;
    SDL_Event event;

    auto lastTime = std::chrono::high_resolution_clock::now();
    auto lastFrameTime = lastTime;
    int fpsFrameCount = 0;
    float fps = 0.0f;

    while (running) {
        auto currentFrameTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
        lastFrameTime = currentFrameTime;

        // Update day/night cycle - NO MORE CHUNK RECALCULATION!
        lighting.update(deltaTime, 1.0f / 3600.0f);

        // Update ChunkManager's tracked light level (for debug/queries)
        // Note: This doesn't recalculate chunks anymore, just tracks the value!
        chunkManager.setGlobalSkyLightLevel(lighting.getSkyLightLevel());

        fpsFrameCount++;
        auto currentTime = std::chrono::high_resolution_clock::now();
        float fpsTime = std::chrono::duration<float>(currentTime - lastTime).count();
        if (fpsTime >= 1.0f) {
            fps = fpsFrameCount / fpsTime;
            fpsFrameCount = 0;
            lastTime = currentTime;
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    window.togglePause();
                }
                if (event.key.key == SDLK_F3) {
                    debugOverlay.toggle();
                }
                if (event.key.key == SDLK_F1) {
                    GameMode newMode = (player.getGameMode() == GameMode::SPECTATOR)
                        ? GameMode::SURVIVAL
                        : GameMode::SPECTATOR;
                    player.setGameMode(newMode);
                    std::cout << "Switched to "
                        << (newMode == GameMode::SPECTATOR ? "SPECTATOR" : "SURVIVAL")
                        << " mode" << std::endl;
                }

                if (event.key.key == SDLK_1) hud.setSelectedSlot(0);
                if (event.key.key == SDLK_2) hud.setSelectedSlot(1);
                if (event.key.key == SDLK_3) hud.setSelectedSlot(2);
                if (event.key.key == SDLK_4) hud.setSelectedSlot(3);
                if (event.key.key == SDLK_5) hud.setSelectedSlot(4);
                if (event.key.key == SDLK_6) hud.setSelectedSlot(5);
                if (event.key.key == SDLK_7) hud.setSelectedSlot(6);
                if (event.key.key == SDLK_8) hud.setSelectedSlot(7);
                if (event.key.key == SDLK_9) hud.setSelectedSlot(8);
                if (event.key.key == SDLK_0) hud.setSelectedSlot(9);
            }

            if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                if (!window.isPaused()) {
                    if (event.wheel.y > 0) {
                        int newSlot = hud.getSelectedSlot() - 1;
                        if (newSlot < 0) newSlot = 9;
                        hud.setSelectedSlot(newSlot);
                    }
                    else if (event.wheel.y < 0) {
                        int newSlot = hud.getSelectedSlot() + 1;
                        if (newSlot > 9) newSlot = 0;
                        hud.setSelectedSlot(newSlot);
                    }
                }
            }

            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !window.isPaused()) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    blockInteraction.breakBlock(camera, &chunkManager);
                }
                else if (event.button.button == SDL_BUTTON_RIGHT) {
                    if (hud.hasBlockInSlot()) {
                        BlockType selectedBlock = hud.getSelectedBlock();
                        blockInteraction.placeBlock(camera, &chunkManager, selectedBlock);
                    }
                }
            }

            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && window.isPaused()) {
                int buttonID;
                if (pauseMenu.isButtonClicked(event.button.x, event.button.y,
                    window.getWidth(), window.getHeight(), buttonID)) {
                    if (buttonID == PauseMenu::BUTTON_RESUME) {
                        window.togglePause();
                    }
                    else if (buttonID == PauseMenu::BUTTON_FULLSCREEN) {
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

        if (!window.isPaused()) {
            const bool* keyState = SDL_GetKeyboardState(nullptr);
            float deltaFront = 0.0f, deltaRight = 0.0f, deltaUp = 0.0f;
            bool jump = false;
            bool sprint = false;

            sprint = keyState[SDL_SCANCODE_LSHIFT] || keyState[SDL_SCANCODE_RSHIFT];

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
            player.update(deltaTime, &chunkManager, camera);
            chunkManager.update(player.x, player.z);
        }

        glm::vec3 sky = lighting.getSkyColor();
        glClearColor(sky.r, sky.g, sky.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        // GPU OPTIMIZATION: Pass global sky light level to shader!
        // This one uniform updates ALL chunks simultaneously on the GPU
        float globalSkyLight = static_cast<float>(lighting.getSkyLightLevel());
        glUniform1f(glGetUniformLocation(shader.getID(), "globalSkyLightLevel"), globalSkyLight);

        float model[16], view[16], projection[16];
        identityMatrix(model);

        lookAtMatrix(view,
            camera.x, camera.y, camera.z,
            camera.x + camera.frontX, camera.y + camera.frontY, camera.z + camera.frontZ,
            0.0f, 1.0f, 0.0f);

        perspectiveMatrix(projection, 70.0f * 3.14159f / 180.0f,
            (float)window.getWidth() / (float)window.getHeight(), 0.1f, 3000.0f);

        unsigned int modelLoc = glGetUniformLocation(shader.getID(), "model");
        unsigned int viewLoc = glGetUniformLocation(shader.getID(), "view");
        unsigned int projLoc = glGetUniformLocation(shader.getID(), "projection");

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection);

        grassBlockTexture.bind();
        chunkManager.renderType(BlockType::GRASS);

        dirtBlockTexture.bind();
        chunkManager.renderType(BlockType::DIRT);

        stoneBlockTexture.bind();
        chunkManager.renderType(BlockType::STONE);

        sandBlockTexture.bind();
        chunkManager.renderType(BlockType::SAND);

		blockOfPureWhiteLightTexture.bind();
		chunkManager.renderType(BlockType::BLOCKOFPUREWHITELIGHT);

		blockOfPureRedLightTexture.bind();
		chunkManager.renderType(BlockType::BLOCKOFPUREREDLIGHT);

		blockOfPureGreenLightTexture.bind();
		chunkManager.renderType(BlockType::BLOCKOFPUREGREENLIGHT);

		blockOfPureBlueLightTexture.bind();
		chunkManager.renderType(BlockType::BLOCKOFPUREBLUELIGHT);


        skybox.render(view, projection, lighting.getTimeOfDay());

        if (!window.isPaused()) {
            blockOutline.render(camera, &chunkManager, view, projection);
        }

        if (!window.isPaused()) {
            crosshair.render(window.getWidth(), window.getHeight());
        }

        if (!window.isPaused()) {
            hud.render(window.getWidth(), window.getHeight());
        }

        debugOverlay.render(window.getWidth(), window.getHeight(),
            camera.x, camera.y, camera.z,
            camera.yaw, fps, lighting.getTimeOfDay(), &chunkManager);

        if (window.isPaused()) {
            pauseMenu.render(window.getWidth(), window.getHeight());
        }

        window.swapBuffers();
    }

    SDL_Quit();
    return 0;
}