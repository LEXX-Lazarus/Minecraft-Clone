#ifndef DEBUG_OVERLAY_H
#define DEBUG_OVERLAY_H

#include <string>

class DebugOverlay {
public:
    DebugOverlay();
    ~DebugOverlay();

    void initialize();
    void render(int windowWidth, int windowHeight,
        float posX, float posY, float posZ,
        float yaw, float fps);

    void toggle() { enabled = !enabled; }
    bool isEnabled() const { return enabled; }

private:
    bool enabled;
    unsigned int VAO, VBO;
    unsigned int shaderProgram;
    unsigned int fontTexture;
    unsigned char* fontBitmap;

    struct CharInfo {
        float x0, y0, x1, y1;
        float xoff, yoff, xadvance;
    };
    CharInfo charInfo[128];

    void setupMesh();
    unsigned int createTextShader();
    bool loadFont(const char* fontPath);
    void renderText(const std::string& text, float x, float y, float scale,
        int windowWidth, int windowHeight);
    std::string getCardinalDirection(float yaw);
};

#endif