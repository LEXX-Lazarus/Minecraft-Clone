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

    std::string getCardinalDirection(float yaw);
};

#endif