#pragma once
#include <string>

// Forward declaration
class ChunkManager;

struct CharInfo {
    float x0, y0, x1, y1;
    float xoff, yoff, xadvance;
};

class DebugOverlay {
public:
    DebugOverlay();
    ~DebugOverlay();
    void initialize();
    void toggle() { enabled = !enabled; }
    bool isEnabled() const { return enabled; }
    void render(int windowWidth, int windowHeight,
        float posX, float posY, float posZ,
        float yaw, float fps, float timeOfDay,
        ChunkManager* chunkManager);  // ADDED chunkManager parameter

private:
    bool enabled;
    unsigned int VAO;
    unsigned int VBO;
    unsigned int shaderProgram;
    unsigned int fontTexture;
    unsigned char* fontBitmap;
    CharInfo charInfo[128];

    void setupMesh();
    unsigned int createTextShader();
    bool loadFont(const char* fontPath);
    void renderText(const std::string& text, float x, float y, float scale,
        int windowWidth, int windowHeight);
    std::string getCardinalDirection(float yaw);
    std::string formatTimeOfDay(float timeOfDay);
};