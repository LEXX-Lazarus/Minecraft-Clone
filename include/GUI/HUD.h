#ifndef HUD_H
#define HUD_H

#include <glad/glad.h>
#include "Block.h"
#include "Rendering/Texture.h"

class HUD {
public:
    HUD();
    ~HUD();

    void initialize();
    void render(int screenWidth, int screenHeight);

    // Hotbar management
    void setSelectedSlot(int slot);  // 0-9 for slots 1-10
    int getSelectedSlot() const { return selectedSlot; }
    BlockType getSelectedBlock() const;

    bool hasBlockInSlot() const;  // Check if current slot has a block

private:
    void renderHotbar(int screenWidth, int screenHeight);
    void setupQuad();
    void createShader();
    void renderQuad(float x, float y, float width, float height, int screenWidth, int screenHeight);

    // OpenGL objects
    GLuint VAO, VBO, EBO;
    GLuint shaderProgram;

    // Textures
    Texture* hotbarTexture;
    Texture* selectedSlotTexture;

    // Hotbar state
    int selectedSlot;  // 0-9 (represents slots 1-10)
    BlockType hotbarSlots[10];  // What block is in each slot (AIR = empty)
};

#endif