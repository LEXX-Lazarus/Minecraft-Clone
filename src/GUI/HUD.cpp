#include "GUI/HUD.h"
#include <iostream>

HUD::HUD()
    : VAO(0), VBO(0), EBO(0), shaderProgram(0),
    hotbarTexture(nullptr), selectedSlotTexture(nullptr),
    selectedSlot(0)
{
    // Initialize hotbar slots - first 4 have blocks, rest are empty
    hotbarSlots[0] = BlockType::DIRT;   // Slot 1
    hotbarSlots[1] = BlockType::GRASS;  // Slot 2
    hotbarSlots[2] = BlockType::STONE;  // Slot 3
    hotbarSlots[3] = BlockType::SAND;   // Slot 4
    hotbarSlots[4] = BlockType::AIR;    // Slot 5 (empty)
    hotbarSlots[5] = BlockType::AIR;    // Slot 6 (empty)
    hotbarSlots[6] = BlockType::AIR;    // Slot 7 (empty)
    hotbarSlots[7] = BlockType::AIR;    // Slot 8 (empty)
    hotbarSlots[8] = BlockType::AIR;    // Slot 9 (empty)
    hotbarSlots[9] = BlockType::AIR;    // Slot 10 (empty)
}

HUD::~HUD() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (EBO) glDeleteBuffers(1, &EBO);
    if (shaderProgram) glDeleteProgram(shaderProgram);

    delete hotbarTexture;
    delete selectedSlotTexture;
}

void HUD::initialize() {
    // Load textures
    hotbarTexture = new Texture("assets/textures/gui/10SlotHotBar.png");
    selectedSlotTexture = new Texture("assets/textures/gui/SelectedHotBarSlot.png");

    createShader();
    setupQuad();

    std::cout << "HUD initialized with 10-slot hotbar" << std::endl;
}

void HUD::createShader() {
    // Simple 2D GUI shader
    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoord;
        
        out vec2 TexCoord;
        
        uniform mat4 projection;
        
        void main() {
            gl_Position = projection * vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 330 core
        out vec4 FragColor;
        
        in vec2 TexCoord;
        
        uniform sampler2D texture1;
        
        void main() {
            FragColor = texture(texture1, TexCoord);
        }
    )";

    // Compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    // Compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    // Link shaders
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void HUD::setupQuad() {
    float vertices[] = {
        // positions   // texture coords
        0.0f, 1.0f,    0.0f, 1.0f,
        1.0f, 0.0f,    1.0f, 0.0f,
        0.0f, 0.0f,    0.0f, 0.0f,

        0.0f, 1.0f,    0.0f, 1.0f,
        1.0f, 1.0f,    1.0f, 1.0f,
        1.0f, 0.0f,    1.0f, 0.0f
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void HUD::render(int screenWidth, int screenHeight) {
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    renderHotbar(screenWidth, screenHeight);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void HUD::renderQuad(float x, float y, float width, float height, int screenWidth, int screenHeight) {
    glUseProgram(shaderProgram);

    // Create orthographic projection matrix (0,0 at top-left)
    float left = 0.0f;
    float right = static_cast<float>(screenWidth);
    float bottom = static_cast<float>(screenHeight);
    float top = 0.0f;

    float ortho[16] = {
        2.0f / (right - left), 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -(right + left) / (right - left), -(top + bottom) / (top - bottom), 0.0f, 1.0f
    };

    GLuint projLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, ortho);

    // Create model matrix for this quad
    float model[16] = {
        width, 0.0f, 0.0f, 0.0f,
        0.0f, height, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        x, y, 0.0f, 1.0f
    };

    // Combine projection and model (manual multiply for position)
    float combined[16];
    for (int i = 0; i < 16; i++) combined[i] = ortho[i];
    combined[12] = ortho[0] * x + ortho[12];
    combined[13] = ortho[5] * y + ortho[13];

    // Scale
    combined[0] = ortho[0] * width;
    combined[5] = ortho[5] * height;

    glUniformMatrix4fv(projLoc, 1, GL_FALSE, combined);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void HUD::renderHotbar(int screenWidth, int screenHeight) {
    // Base dimensions at 1920x1080 reference resolution
    const int baseWidth = 800;
    const int baseHeight = 80;
    const int referenceWidth = 1920;

    // Scale based on screen width
    float scale = static_cast<float>(screenWidth) / static_cast<float>(referenceWidth);
    int hotbarWidth = static_cast<int>(baseWidth * scale);
    int hotbarHeight = static_cast<int>(baseHeight * scale);

    // Position at bottom center - 5px from bottom (also scaled)
    int xPos = (screenWidth - hotbarWidth) / 2;
    int yPos = screenHeight - hotbarHeight - static_cast<int>(5 * scale);

    // Render hotbar background
    hotbarTexture->bind();
    renderQuad(static_cast<float>(xPos), static_cast<float>(yPos),
        static_cast<float>(hotbarWidth), static_cast<float>(hotbarHeight),
        screenWidth, screenHeight);

    // Render selected slot highlight
    int slotWidth = hotbarWidth / 10;
    int selectedX = xPos + (selectedSlot * slotWidth) - static_cast<int>(2 * scale);
    int selectedY = yPos - static_cast<int>(2 * scale);
    int highlightWidth = slotWidth + static_cast<int>(4 * scale);
    int highlightHeight = hotbarHeight + static_cast<int>(4 * scale);

    selectedSlotTexture->bind();
    renderQuad(static_cast<float>(selectedX), static_cast<float>(selectedY),
        static_cast<float>(highlightWidth), static_cast<float>(highlightHeight),
        screenWidth, screenHeight);
}

void HUD::setSelectedSlot(int slot) {
    if (slot >= 0 && slot < 10) {
        selectedSlot = slot;
    }
}

BlockType HUD::getSelectedBlock() const {
    return hotbarSlots[selectedSlot];
}

bool HUD::hasBlockInSlot() const {
    return hotbarSlots[selectedSlot] != BlockType::AIR;
}