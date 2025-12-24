#include "GUI/DebugOverlay.h"
#include <glad/glad.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <iostream>
#include <sstream>
#include <iomanip>

DebugOverlay::DebugOverlay()
    : enabled(true), VAO(0), VBO(0), shaderProgram(0),
    fontTexture(0), fontBitmap(nullptr) {
}

DebugOverlay::~DebugOverlay() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (shaderProgram) glDeleteProgram(shaderProgram);
    if (fontTexture) glDeleteTextures(1, &fontTexture);
    if (fontBitmap) delete[] fontBitmap;
}

void DebugOverlay::initialize() {
    shaderProgram = createTextShader();
    loadFont("assets/fonts/Perfect DOS VGA 437.ttf");
    setupMesh();
}

void DebugOverlay::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

unsigned int DebugOverlay::createTextShader() {
    const char* vertexShader = R"(
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

    const char* fragmentShader = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D fontTexture;
uniform vec3 textColor;

void main()
{
    float alpha = texture(fontTexture, TexCoord).r;
    FragColor = vec4(textColor, alpha);
}
)";

    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShader, nullptr);
    glCompileShader(vs);

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShader, nullptr);
    glCompileShader(fs);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

bool DebugOverlay::loadFont(const char* fontPath) {
    FILE* fontFile = fopen(fontPath, "rb");
    if (!fontFile) {
        std::cerr << "Failed to open font: " << fontPath << std::endl;
        return false;
    }

    fseek(fontFile, 0, SEEK_END);
    size_t fileSize = ftell(fontFile);
    fseek(fontFile, 0, SEEK_SET);

    unsigned char* fontBuffer = new unsigned char[fileSize];
    fread(fontBuffer, 1, fileSize, fontFile);
    fclose(fontFile);

    const int bitmapWidth = 512;
    const int bitmapHeight = 512;
    fontBitmap = new unsigned char[bitmapWidth * bitmapHeight];

    stbtt_bakedchar cdata[96];
    stbtt_BakeFontBitmap(fontBuffer, 0, 32.0, fontBitmap, bitmapWidth, bitmapHeight, 32, 96, cdata);

    for (int i = 0; i < 96; i++) {
        charInfo[i + 32].x0 = cdata[i].x0 / (float)bitmapWidth;
        charInfo[i + 32].y0 = cdata[i].y0 / (float)bitmapHeight;
        charInfo[i + 32].x1 = cdata[i].x1 / (float)bitmapWidth;
        charInfo[i + 32].y1 = cdata[i].y1 / (float)bitmapHeight;
        charInfo[i + 32].xoff = cdata[i].xoff;
        charInfo[i + 32].yoff = cdata[i].yoff;
        charInfo[i + 32].xadvance = cdata[i].xadvance;
    }

    glGenTextures(1, &fontTexture);
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, bitmapWidth, bitmapHeight, 0, GL_RED, GL_UNSIGNED_BYTE, fontBitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    delete[] fontBuffer;
    return true;
}

void DebugOverlay::renderText(const std::string& text, float x, float y, float scale,
    int windowWidth, int windowHeight) {
    glUseProgram(shaderProgram);
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    glBindVertexArray(VAO);

    // BRIGHT YELLOW
    glUniform3f(glGetUniformLocation(shaderProgram, "textColor"), 1.0f, 1.0f, 0.0f);

    for (char c : text) {
        if (c < 32 || c > 126) continue;

        CharInfo& ch = charInfo[(int)c];

        float xpos = x + ch.xoff * scale;
        float ypos = y + ch.yoff * scale;
        float w = (ch.x1 - ch.x0) * 512 * scale;
        float h = (ch.y1 - ch.y0) * 512 * scale;

        float x1 = (xpos / windowWidth) * 2.0f - 1.0f;
        float y1 = 1.0f - (ypos / windowHeight) * 2.0f;
        float x2 = ((xpos + w) / windowWidth) * 2.0f - 1.0f;
        float y2 = 1.0f - ((ypos + h) / windowHeight) * 2.0f;

        float vertices[] = {
            x1, y1, ch.x0, ch.y0,
            x2, y1, ch.x1, ch.y0,
            x2, y2, ch.x1, ch.y1,

            x1, y1, ch.x0, ch.y0,
            x2, y2, ch.x1, ch.y1,
            x1, y2, ch.x0, ch.y1
        };

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += ch.xadvance * scale;
    }

    glBindVertexArray(0);
}

std::string DebugOverlay::getCardinalDirection(float yaw) {
    // Normalize yaw to 0-360
    while (yaw < 0) yaw += 360.0f;
    while (yaw >= 360) yaw -= 360.0f;

    // With negated Z: yaw -90/270 faces +X (East), yaw 0 faces +Z (North)
    if (yaw >= 315 || yaw < 45) return "East";
    else if (yaw >= 45 && yaw < 135) return "North";
    else if (yaw >= 135 && yaw < 225) return "West";
    else return "South";
}

std::string DebugOverlay::formatTimeOfDay(float timeOfDay) {
    // timeOfDay: 0.0 = midnight, 0.25 = 6am, 0.5 = noon, 0.75 = 6pm
    // Convert to 24-hour time
    float totalMinutes = timeOfDay * 24.0f * 60.0f;
    int hours = (int)(totalMinutes / 60.0f);
    int minutes = (int)(totalMinutes) % 60;

    // Convert to 12-hour format
    bool isPM = hours >= 12;
    int displayHours = hours % 12;
    if (displayHours == 0) displayHours = 12;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << displayHours << ":"
        << std::setfill('0') << std::setw(2) << minutes << " "
        << (isPM ? "PM" : "AM");

    return oss.str();
}

void DebugOverlay::render(int windowWidth, int windowHeight,
    float posX, float posY, float posZ,
    float yaw, float fps, float timeOfDay) {
    if (!enabled) return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    std::string posText = "Position: " + std::to_string((int)posX) + ", " +
        std::to_string((int)posY) + ", " + std::to_string((int)(-posZ));
    std::string dirText = "Direction: " + getCardinalDirection(yaw);
    std::string timeText = "Time: " + formatTimeOfDay(timeOfDay);
    std::string yawText = "Yaw: " + std::to_string((int)yaw);
    std::string fpsText = "FPS: " + std::to_string((int)fps);

    // NEW ORDER: Position, Direction, Time, Yaw, FPS
    renderText(posText, 10, 50, 1.2f, windowWidth, windowHeight);
    renderText(dirText, 10, 80, 1.2f, windowWidth, windowHeight);
    renderText(timeText, 10, 110, 1.2f, windowWidth, windowHeight);
    renderText(yawText, 10, 140, 1.2f, windowWidth, windowHeight);
    renderText(fpsText, 10, 170, 1.2f, windowWidth, windowHeight);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}