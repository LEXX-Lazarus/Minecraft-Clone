#pragma once

#include <glad/glad.h>

class Camera;
class ChunkManager;

class BlockOutline {
public:
    BlockOutline();
    ~BlockOutline();

    void initialize();

    void render(
        Camera& camera,
        ChunkManager* chunkManager,
        float* viewMatrix,
        float* projectionMatrix
    );

private:
    unsigned int VAO;
    unsigned int VBO;
    unsigned int shaderProgram;

    int selectedBlockX;
    int selectedBlockY;
    int selectedBlockZ;

    int hitFace;
    bool hasSelection;

    void setupMesh();
    unsigned int createShader();

    bool raycastBlock(Camera& camera, ChunkManager* chunkManager);
};
