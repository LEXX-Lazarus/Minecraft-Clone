#ifndef GUI_H
#define GUI_H

#include <glad/glad.h>

class GUI {
public:
    GUI();
    ~GUI();

    void initialize();
    void renderPauseMenu(int windowWidth, int windowHeight);
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
    unsigned int resumeButtonTexture;  // Make sure this line exists!

    void setupMesh();
    unsigned int createGUIShader();
    unsigned int createTexturedGUIShader();
    unsigned int loadButtonTexture(const char* path);
};

#endif