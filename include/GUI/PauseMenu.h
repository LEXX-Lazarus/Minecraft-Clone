#ifndef PAUSE_MENU_H
#define PAUSE_MENU_H

#include <glad/glad.h>

class PauseMenu {
public:
    PauseMenu();
    ~PauseMenu();

    void initialize();
    void render(int windowWidth, int windowHeight);
    bool isButtonClicked(int mouseX, int mouseY, int windowWidth, int windowHeight, int& buttonID);

    enum ButtonID {
        BUTTON_RESUME = 1,
        BUTTON_FULLSCREEN = 2
    };

private:
    unsigned int VAO, VBO;
    unsigned int shaderProgram;
    unsigned int texturedShaderProgram;
    unsigned int fullscreenButtonTexture;
    unsigned int resumeButtonTexture;

    void setupMesh();
    unsigned int createShader();
    unsigned int createTexturedShader();
    unsigned int loadButtonTexture(const char* path);
};

#endif