#include "GUI/Crosshair.h"
#include <iostream>
#include <cmath>
#include <vector>  

// Vertex shader for crosshair
const char* crosshairVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

out vec3 vertexColor;

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    vertexColor = aColor;
}
)";

// Fragment shader for crosshair with vertex colors
const char* crosshairFragmentShader = R"(
#version 330 core
out vec4 FragColor;

in vec3 vertexColor;

void main()
{
    FragColor = vec4(vertexColor, 1.0);
}
)";

Crosshair::Crosshair()
    : VAO(0), VBO(0), shaderProgram(0), centerPixelFBO(0), centerPixelTexture(0) {
    crosshairColor[0] = 1.0f;
    crosshairColor[1] = 1.0f;
    crosshairColor[2] = 1.0f;
}

Crosshair::~Crosshair() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (shaderProgram) glDeleteProgram(shaderProgram);
    if (centerPixelFBO) glDeleteFramebuffers(1, &centerPixelFBO);
    if (centerPixelTexture) glDeleteTextures(1, &centerPixelTexture);
}

void Crosshair::initialize() {
    shaderProgram = createShader();
    setupMesh();
}

void Crosshair::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Position attribute (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Color attribute (r, g, b)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

unsigned int Crosshair::createShader() {
    // Compile vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &crosshairVertexShader, nullptr);
    glCompileShader(vertexShader);

    int success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Crosshair Vertex shader compilation failed:\n" << infoLog << std::endl;
    }

    // Compile fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &crosshairFragmentShader, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Crosshair Fragment shader compilation failed:\n" << infoLog << std::endl;
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
        std::cerr << "Crosshair Shader program linking failed:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

void Crosshair::sampleCenterPixel(int windowWidth, int windowHeight) {
    // Read the pixel at the center of the screen
    int centerX = windowWidth / 2;
    int centerY = windowHeight / 2;

    unsigned char pixel[3];
    glReadPixels(centerX, centerY, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);

    // Convert to 0-1 range
    float r = pixel[0] / 255.0f;
    float g = pixel[1] / 255.0f;
    float b = pixel[2] / 255.0f;

    calculateAdaptiveColor(r, g, b);
}

void Crosshair::calculateAdaptiveColor(float r, float g, float b) {
    // Calculate perceived brightness (human eye is more sensitive to green)
    float brightness = 0.299f * r + 0.587f * g + 0.114f * b;

    // Apply bias towards extremes using a power curve
    float adjusted = brightness;
    if (brightness < 0.5f) {
        adjusted = std::pow(brightness * 2.0f, 2.5f) * 0.5f;
    }
    else {
        adjusted = 1.0f - std::pow((1.0f - brightness) * 2.0f, 2.5f) * 0.5f;
    }

    // Interior: inverted (dark background = white crosshair)
    float interior = 1.0f - adjusted;

    // Apply contrast boost
    if (interior < 0.5f) {
        interior = std::max(0.0f, interior - 0.1f);
    }
    else {
        interior = std::min(1.0f, interior + 0.1f);
    }

    // Border: opposite of interior for maximum contrast
    float border = adjusted;
    if (border < 0.5f) {
        border = std::max(0.0f, border - 0.1f);
    }
    else {
        border = std::min(1.0f, border + 0.1f);
    }

    // Store both colors
    crosshairColor[0] = interior;  // Interior color
    crosshairColor[1] = border;    // Border color
    crosshairColor[2] = 0.0f;      // Unused
}

void Crosshair::render(int windowWidth, int windowHeight) {
    // Sample the center pixel to determine crosshair color
    sampleCenterPixel(windowWidth, windowHeight);

    // Disable depth test for 2D overlay
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);

    // Crosshair dimensions (larger)
    float armLength = 0.025f;       // Length of each arm
    float armThickness = 0.004f;    // Thickness of crosshair
    float gapFromCenter = 0.008f;   // Gap from center
    float borderWidth = 0.0015f;    // Border thickness

    // Aspect ratio correction
    float aspectRatio = (float)windowWidth / (float)windowHeight;

    // Colors
    float interiorR = crosshairColor[0];
    float interiorG = crosshairColor[0];
    float interiorB = crosshairColor[0];

    float borderR = crosshairColor[1];
    float borderG = crosshairColor[1];
    float borderB = crosshairColor[1];

    std::vector<float> vertices;

    // Helper lambda to add a rectangle with color
    auto addRect = [&](float x1, float y1, float x2, float y2, float r, float g, float b) {
        // Triangle 1
        vertices.push_back(x1); vertices.push_back(y1); vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);
        vertices.push_back(x2); vertices.push_back(y1); vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);
        vertices.push_back(x2); vertices.push_back(y2); vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);
        // Triangle 2
        vertices.push_back(x1); vertices.push_back(y1); vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);
        vertices.push_back(x2); vertices.push_back(y2); vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);
        vertices.push_back(x1); vertices.push_back(y2); vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);
        };

    // ===== VERTICAL ARMS (aspect corrected) =====

    // TOP ARM (with border)
    float topStart = gapFromCenter;
    float topEnd = gapFromCenter + armLength;
    float leftEdge = -armThickness / aspectRatio;  // FIXED: aspect correction
    float rightEdge = armThickness / aspectRatio;   // FIXED: aspect correction

    // Top arm border (outer)
    addRect(leftEdge - borderWidth / aspectRatio, topStart - borderWidth,  // FIXED: aspect correction
        rightEdge + borderWidth / aspectRatio, topEnd + borderWidth,    // FIXED: aspect correction
        borderR, borderG, borderB);

    // Top arm interior
    addRect(leftEdge, topStart, rightEdge, topEnd, interiorR, interiorG, interiorB);

    // BOTTOM ARM (with border)
    float bottomStart = -gapFromCenter;
    float bottomEnd = -gapFromCenter - armLength;

    // Bottom arm border (outer)
    addRect(leftEdge - borderWidth / aspectRatio, bottomEnd - borderWidth,  // FIXED: aspect correction
        rightEdge + borderWidth / aspectRatio, bottomStart + borderWidth, // FIXED: aspect correction
        borderR, borderG, borderB);

    // Bottom arm interior
    addRect(leftEdge, bottomEnd, rightEdge, bottomStart, interiorR, interiorG, interiorB);

    // ===== HORIZONTAL ARMS (aspect corrected) =====

    float topEdge = armThickness;
    float bottomEdge = -armThickness;

    // LEFT ARM (with border)
    float leftStart = -gapFromCenter / aspectRatio;
    float leftEnd = (-gapFromCenter - armLength) / aspectRatio;

    // Left arm border (outer)
    addRect(leftEnd - borderWidth / aspectRatio, bottomEdge - borderWidth,
        leftStart + borderWidth / aspectRatio, topEdge + borderWidth,
        borderR, borderG, borderB);

    // Left arm interior
    addRect(leftEnd, bottomEdge, leftStart, topEdge, interiorR, interiorG, interiorB);

    // RIGHT ARM (with border)
    float rightStart = gapFromCenter / aspectRatio;
    float rightEnd = (gapFromCenter + armLength) / aspectRatio;

    // Right arm border (outer)
    addRect(rightStart - borderWidth / aspectRatio, bottomEdge - borderWidth,
        rightEnd + borderWidth / aspectRatio, topEdge + borderWidth,
        borderR, borderG, borderB);

    // Right arm interior
    addRect(rightStart, bottomEdge, rightEnd, topEdge, interiorR, interiorG, interiorB);

    // Upload vertices and render
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 5);

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}