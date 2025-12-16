#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <iostream>
#include <vector>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed:\n" << infoLog << std::endl;
    }
    return shader;
}

unsigned int createShaderProgram() {
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Program linking failed:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

unsigned int loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Load image
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);

    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        std::cout << "Texture loaded: " << path << " (" << width << "x" << height << ")" << std::endl;
    }
    else {
        std::cerr << "Failed to load texture: " << path << std::endl;
    }

    stbi_image_free(data);
    return textureID;
}

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow(
        "Minecraft Clone",
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        SDL_GL_DestroyContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    SDL_GL_SetSwapInterval(1);
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Capture mouse for FPS controls
    SDL_SetWindowRelativeMouseMode(window, true);

    // Cube vertices - positioned at world origin (0, 0, 0)
    float vertices[] = {
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

    unsigned int indices[] = {
        0, 1, 2, 2, 3, 0,       // Top
        4, 5, 6, 6, 7, 4,       // Bottom
        8, 9, 10, 10, 11, 8,    // South
        12, 13, 14, 14, 15, 12, // North
        16, 17, 18, 18, 19, 16, // East
        20, 21, 22, 22, 23, 20  // West
    };

    unsigned int VAO, VBO, EBO;
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

    unsigned int shaderProgram = createShaderProgram();
    unsigned int texture = loadTexture("assets/textures/GrassBlock.png");

    // Camera variables
    float cameraX = 0.0f;
    float cameraY = 1.5f;
    float cameraZ = 3.0f;
    float cameraSpeed = 0.05f;

    // Camera rotation (yaw and pitch)
    float yaw = -90.0f;   // Looking towards -Z initially
    float pitch = 0.0f;
    float mouseSensitivity = 0.1f;

    // Camera direction vectors
    float frontX = 0.0f, frontY = 0.0f, frontZ = -1.0f;
    float rightX = 1.0f, rightY = 0.0f, rightZ = 0.0f;

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                float xoffset = event.motion.xrel * mouseSensitivity;
                float yoffset = -event.motion.yrel * mouseSensitivity;

                yaw += xoffset;
                pitch += yoffset;

                // Constrain pitch
                if (pitch > 89.0f) pitch = 89.0f;
                if (pitch < -89.0f) pitch = -89.0f;

                // Update camera direction
                float yawRad = yaw * 3.14159f / 180.0f;
                float pitchRad = pitch * 3.14159f / 180.0f;

                frontX = cos(yawRad) * cos(pitchRad);
                frontY = sin(pitchRad);
                frontZ = sin(yawRad) * cos(pitchRad);

                // Normalize front vector
                float length = sqrt(frontX * frontX + frontY * frontY + frontZ * frontZ);
                frontX /= length;
                frontY /= length;
                frontZ /= length;

                // Calculate right vector (cross product of front and world up)
                rightX = frontY * 0.0f - frontZ * 1.0f;
                rightY = frontZ * 0.0f - frontX * 0.0f;
                rightZ = frontX * 1.0f - frontY * 0.0f;

                // Normalize right vector
                length = sqrt(rightX * rightX + rightY * rightY + rightZ * rightZ);
                rightX /= length;
                rightY /= length;
                rightZ /= length;
            }
        }

        // WASD movement (relative to camera direction)
        const bool* keyState = SDL_GetKeyboardState(nullptr);
        if (keyState[SDL_SCANCODE_W]) {
            cameraX += frontX * cameraSpeed;
            cameraY += frontY * cameraSpeed;
            cameraZ += frontZ * cameraSpeed;
        }
        if (keyState[SDL_SCANCODE_S]) {
            cameraX -= frontX * cameraSpeed;
            cameraY -= frontY * cameraSpeed;
            cameraZ -= frontZ * cameraSpeed;
        }
        if (keyState[SDL_SCANCODE_A]) {
            cameraX -= rightX * cameraSpeed;
            cameraY -= rightY * cameraSpeed;
            cameraZ -= rightZ * cameraSpeed;
        }
        if (keyState[SDL_SCANCODE_D]) {
            cameraX += rightX * cameraSpeed;
            cameraY += rightY * cameraSpeed;
            cameraZ += rightZ * cameraSpeed;
        }
        if (keyState[SDL_SCANCODE_SPACE]) {
            cameraY += cameraSpeed;
        }
        if (keyState[SDL_SCANCODE_LSHIFT]) {
            cameraY -= cameraSpeed;
        }

        glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindTexture(GL_TEXTURE_2D, texture);

        // Create matrices
        float model[16], view[16], projection[16];

        // Model matrix (identity - cube stays at 0,0,0)
        identityMatrix(model);

        // View matrix (camera looking at front direction)
        lookAtMatrix(view,
            cameraX, cameraY, cameraZ,
            cameraX + frontX, cameraY + frontY, cameraZ + frontZ,
            0.0f, 1.0f, 0.0f);

        // Projection matrix
        perspectiveMatrix(projection, 45.0f * 3.14159f / 180.0f, 800.0f / 600.0f, 0.1f, 100.0f);

        // Set uniforms
        unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
        unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
        unsigned int projLoc = glGetUniformLocation(shaderProgram, "projection");

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        SDL_GL_SwapWindow(window);
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);

    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}