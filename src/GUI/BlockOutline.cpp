#include "GUI/BlockOutline.h"
#include "Player/Camera.h"
#include "World/ChunkManager.h"
#include "Block.h"
#include <iostream>
#include <cmath>
#include <vector>

// Vertex shader for block outline
const char* outlineVertexShader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

// Fragment shader for block outline
const char* outlineFragmentShader = R"(
#version 330 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(0.0, 0.0, 0.0, 1.0);  // Solid black
}
)";

BlockOutline::BlockOutline()
    : VAO(0), VBO(0), shaderProgram(0),
    selectedBlockX(0), selectedBlockY(0), selectedBlockZ(0),
    hitFace(-1), hasSelection(false) {
}

BlockOutline::~BlockOutline() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (shaderProgram) glDeleteProgram(shaderProgram);
}

void BlockOutline::initialize() {
    shaderProgram = createShader();
    setupMesh();
}

void BlockOutline::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

unsigned int BlockOutline::createShader() {
    // Compile vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &outlineVertexShader, nullptr);
    glCompileShader(vertexShader);

    int success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "BlockOutline Vertex shader compilation failed:\n" << infoLog << std::endl;
    }

    // Compile fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &outlineFragmentShader, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "BlockOutline Fragment shader compilation failed:\n" << infoLog << std::endl;
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
        std::cerr << "BlockOutline Shader program linking failed:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

bool BlockOutline::raycastBlock(Camera& camera, ChunkManager* chunkManager) {
    if (!chunkManager) return false;

    hasSelection = false;
    hitFace = -1;

    const float maxDistance = 5.0f;
    const float step = 0.05f;

    // Apply offset to detection point
    float rayX = camera.x + 0.0f;  // Offset X
    float rayY = camera.y + 0.5f;  // Offset Y
    float rayZ = camera.z + 0.0f;  // Offset Z

    float dirX = camera.frontX;
    float dirY = camera.frontY;
    float dirZ = camera.frontZ;

    for (float distance = 0.0f; distance < maxDistance; distance += step) {
        rayX += dirX * step;
        rayY += dirY * step;
        rayZ += dirZ * step;

        // Convert to block coordinates
        int blockX = static_cast<int>(std::round(rayX));
        int blockY = static_cast<int>(std::floor(rayY));
        int blockZ = static_cast<int>(std::round(-rayZ));

        Block* block = chunkManager->getBlockAt(blockX, blockY, blockZ);
        if (block && !block->isAir()) {
            selectedBlockX = blockX;
            selectedBlockY = blockY;
            selectedBlockZ = blockZ;
            hasSelection = true;
            delete block;
            return true;
        }
        if (block) delete block;
    }

    return false;
}

void BlockOutline::render(
    Camera& camera,
    ChunkManager* chunkManager,
    float* viewMatrix,
    float* projectionMatrix
) {
    // CRITICAL FIX: Check for selection FIRST, before changing GL state
    if (!raycastBlock(camera, chunkManager)) {
        // No block selected - don't change any GL state!
        return;
    }

    // Save current GL state
    GLboolean depthMaskBefore;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskBefore);

    // Configure for outline rendering
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -1.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(3.0f);

    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);

    std::vector<float> edges;

    // Block world position
    const float bx = static_cast<float>(selectedBlockX);
    const float by = static_cast<float>(selectedBlockY);
    const float bz = static_cast<float>(selectedBlockZ);

    const float rx = bx;
    const float ry = by;
    const float rz = -bz;

    const float o = 0.503f;

    const float viewX = camera.frontX;
    const float viewY = camera.frontY;
    const float viewZ = camera.frontZ;

    auto addEdge = [&](float x1, float y1, float z1,
        float x2, float y2, float z2) {
            edges.insert(edges.end(), { x1, y1, z1, x2, y2, z2 });
        };

    auto isExposed = [&](int dx, int dy, int dz) {
        Block* b = chunkManager->getBlockAt(
            selectedBlockX + dx,
            selectedBlockY + dy,
            selectedBlockZ + dz
        );
        bool exposed = (!b || b->isAir());
        if (b) delete b;
        return exposed;
        };

    auto facingCamera = [&](float nx, float ny, float nz) {
        return (nx * viewX + ny * viewY + nz * viewZ) < 0.0f;
        };

    // TOP (+Y)
    if (isExposed(0, 1, 0) && facingCamera(0.0f, 1.0f, 0.0f)) {
        addEdge(rx - o, ry + o, rz - o, rx + o, ry + o, rz - o);
        addEdge(rx + o, ry + o, rz - o, rx + o, ry + o, rz + o);
        addEdge(rx + o, ry + o, rz + o, rx - o, ry + o, rz + o);
        addEdge(rx - o, ry + o, rz + o, rx - o, ry + o, rz - o);
    }

    // BOTTOM (-Y)
    if (isExposed(0, -1, 0) && facingCamera(0.0f, -1.0f, 0.0f)) {
        addEdge(rx - o, ry - o, rz - o, rx + o, ry - o, rz - o);
        addEdge(rx + o, ry - o, rz - o, rx + o, ry - o, rz + o);
        addEdge(rx + o, ry - o, rz + o, rx - o, ry - o, rz + o);
        addEdge(rx - o, ry - o, rz + o, rx - o, ry - o, rz - o);
    }

    // NORTH (+Z chunk -Z render)
    if (isExposed(0, 0, -1) && facingCamera(0.0f, 0.0f, 1.0f)) {
        addEdge(rx - o, ry - o, rz + o, rx + o, ry - o, rz + o);
        addEdge(rx + o, ry - o, rz + o, rx + o, ry + o, rz + o);
        addEdge(rx + o, ry + o, rz + o, rx - o, ry + o, rz + o);
        addEdge(rx - o, ry + o, rz + o, rx - o, ry - o, rz + o);
    }

    // SOUTH (-Z chunk +Z render)
    if (isExposed(0, 0, 1) && facingCamera(0.0f, 0.0f, -1.0f)) {
        addEdge(rx - o, ry - o, rz - o, rx + o, ry - o, rz - o);
        addEdge(rx + o, ry - o, rz - o, rx + o, ry + o, rz - o);
        addEdge(rx + o, ry + o, rz - o, rx - o, ry + o, rz - o);
        addEdge(rx - o, ry + o, rz - o, rx - o, ry - o, rz - o);
    }

    // EAST (+X)
    if (isExposed(1, 0, 0) && facingCamera(1.0f, 0.0f, 0.0f)) {
        addEdge(rx + o, ry - o, rz - o, rx + o, ry - o, rz + o);
        addEdge(rx + o, ry - o, rz + o, rx + o, ry + o, rz + o);
        addEdge(rx + o, ry + o, rz + o, rx + o, ry + o, rz - o);
        addEdge(rx + o, ry + o, rz - o, rx + o, ry - o, rz - o);
    }

    // WEST (-X)
    if (isExposed(-1, 0, 0) && facingCamera(-1.0f, 0.0f, 0.0f)) {
        addEdge(rx - o, ry - o, rz - o, rx - o, ry - o, rz + o);
        addEdge(rx - o, ry - o, rz + o, rx - o, ry + o, rz + o);
        addEdge(rx - o, ry + o, rz + o, rx - o, ry + o, rz - o);
        addEdge(rx - o, ry + o, rz - o, rx - o, ry - o, rz - o);
    }

    // Render edges if we have any
    if (!edges.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(
            GL_ARRAY_BUFFER,
            edges.size() * sizeof(float),
            edges.data(),
            GL_DYNAMIC_DRAW
        );

        float model[16] = {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0,
            0,0,0,1
        };

        glUniformMatrix4fv(
            glGetUniformLocation(shaderProgram, "model"),
            1, GL_FALSE, model
        );
        glUniformMatrix4fv(
            glGetUniformLocation(shaderProgram, "view"),
            1, GL_FALSE, viewMatrix
        );
        glUniformMatrix4fv(
            glGetUniformLocation(shaderProgram, "projection"),
            1, GL_FALSE, projectionMatrix
        );

        glDrawArrays(GL_LINES, 0, edges.size() / 3);
    }

    // CRITICAL: ALWAYS restore GL state, regardless of whether we drew anything
    glBindVertexArray(0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glLineWidth(1.0f);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glDepthMask(depthMaskBefore);  // Restore original depth mask state
}