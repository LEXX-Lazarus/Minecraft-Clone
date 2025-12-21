#include "GUI/PauseMenu.h"
#include "stb_image.h"
#include <iostream>

// Simple 2D vertex shader
const char* pauseMenuVertexShader = R"(
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
const char* pauseMenuFragmentShader = R"(
#version 330 core
out vec4 FragColor;

in vec4 vertexColor;

void main()
{
    FragColor = vertexColor;
}
)";

// Textured 2D vertex shader
const char* pauseMenuTexturedVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

// Textured 2D fragment shader
const char* pauseMenuTexturedFragmentShader = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D buttonTexture;

void main()
{
    FragColor = texture(buttonTexture, TexCoord);
}
)";

PauseMenu::PauseMenu()
    : VAO(0), VBO(0), shaderProgram(0), texturedShaderProgram(0),
    fullscreenButtonTexture(0), resumeButtonTexture(0) {
}

PauseMenu::~PauseMenu() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (shaderProgram) glDeleteProgram(shaderProgram);
    if (texturedShaderProgram) glDeleteProgram(texturedShaderProgram);
    if (fullscreenButtonTexture) glDeleteTextures(1, &fullscreenButtonTexture);
    if (resumeButtonTexture) glDeleteTextures(1, &resumeButtonTexture);
}

void PauseMenu::initialize() {
    shaderProgram = createShader();
    texturedShaderProgram = createTexturedShader();
    fullscreenButtonTexture = loadButtonTexture("assets/GUI/FullscreenButton.png");
    resumeButtonTexture = loadButtonTexture("assets/GUI/ResumeGameButton.png");
    setupMesh();
}

void PauseMenu::setupMesh() {
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

unsigned int PauseMenu::createShader() {
    // Compile vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &pauseMenuVertexShader, nullptr);
    glCompileShader(vertexShader);

    int success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "PauseMenu Vertex shader compilation failed:\n" << infoLog << std::endl;
    }

    // Compile fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &pauseMenuFragmentShader, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "PauseMenu Fragment shader compilation failed:\n" << infoLog << std::endl;
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
        std::cerr << "PauseMenu Shader program linking failed:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

unsigned int PauseMenu::createTexturedShader() {
    // Compile vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &pauseMenuTexturedVertexShader, nullptr);
    glCompileShader(vertexShader);

    int success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "PauseMenu Textured Vertex shader compilation failed:\n" << infoLog << std::endl;
    }

    // Compile fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &pauseMenuTexturedFragmentShader, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "PauseMenu Textured Fragment shader compilation failed:\n" << infoLog << std::endl;
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
        std::cerr << "PauseMenu Textured Shader program linking failed:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

unsigned int PauseMenu::loadButtonTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);

    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        std::cout << "PauseMenu Button texture loaded: " << path << std::endl;
    }
    else {
        std::cerr << "Failed to load PauseMenu button texture: " << path << std::endl;
    }

    stbi_image_free(data);
    return textureID;
}

void PauseMenu::render(int windowWidth, int windowHeight) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Dark overlay
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);

    float overlayVertices[] = {
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

    // Switch to textured shader for buttons
    glUseProgram(texturedShaderProgram);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Resume button
    glBindTexture(GL_TEXTURE_2D, resumeButtonTexture);

    float resumeVertices[] = {
        -0.3f,  0.15f,  0.0f, 1.0f,
         0.3f,  0.15f,  1.0f, 1.0f,
         0.3f,  0.35f,  1.0f, 0.0f,

        -0.3f,  0.15f,  0.0f, 1.0f,
         0.3f,  0.35f,  1.0f, 0.0f,
        -0.3f,  0.35f,  0.0f, 0.0f
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(resumeVertices), resumeVertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Fullscreen button
    glBindTexture(GL_TEXTURE_2D, fullscreenButtonTexture);

    float fullscreenVertices[] = {
        -0.3f, -0.1f,   0.0f, 1.0f,
         0.3f, -0.1f,   1.0f, 1.0f,
         0.3f,  0.1f,   1.0f, 0.0f,

        -0.3f, -0.1f,   0.0f, 1.0f,
         0.3f,  0.1f,   1.0f, 0.0f,
        -0.3f,  0.1f,   0.0f, 0.0f
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(fullscreenVertices), fullscreenVertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

bool PauseMenu::isButtonClicked(int mouseX, int mouseY, int windowWidth, int windowHeight, int& buttonID) {
    float ndcX = (2.0f * mouseX) / windowWidth - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY) / windowHeight;

    // Resume button bounds
    if (ndcX >= -0.3f && ndcX <= 0.3f && ndcY >= 0.15f && ndcY <= 0.35f) {
        buttonID = BUTTON_RESUME;
        return true;
    }

    // Fullscreen button bounds
    if (ndcX >= -0.3f && ndcX <= 0.3f && ndcY >= -0.1f && ndcY <= 0.1f) {
        buttonID = BUTTON_FULLSCREEN;
        return true;
    }

    return false;
}