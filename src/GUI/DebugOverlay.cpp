#include "GUI/DebugOverlay.h"
#include <iostream>
#include <iomanip>
#include <sstream>

DebugOverlay::DebugOverlay() : enabled(true) {
}

DebugOverlay::~DebugOverlay() {
}

void DebugOverlay::initialize() {
    std::cout << "Debug overlay initialized (console output mode)" << std::endl;
}

std::string DebugOverlay::getCardinalDirection(float yaw) {
    while (yaw < 0) yaw += 360.0f;
    while (yaw >= 360) yaw -= 360.0f;

    if (yaw >= 315 || yaw < 45) return "South";
    else if (yaw >= 45 && yaw < 135) return "West";
    else if (yaw >= 135 && yaw < 225) return "North";
    else return "East";
}

void DebugOverlay::render(int windowWidth, int windowHeight,
    float posX, float posY, float posZ,
    float yaw, float fps) {
    if (!enabled) return;

    // Print to console every 60 frames to avoid spam
    static int frameCounter = 0;
    frameCounter++;

    if (frameCounter % 60 == 0) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1);
        ss << "Position: (" << posX << ", " << posY << ", " << posZ << ") | ";
        ss << "Direction: " << getCardinalDirection(yaw) << " | ";
        ss << "FPS: " << (int)fps;

        std::cout << ss.str() << std::endl;
    }
}