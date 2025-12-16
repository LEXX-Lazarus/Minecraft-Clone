#ifndef WINDOW_H
#define WINDOW_H

#include <SDL3/SDL.h>
#include <string>

class Window {
public:
    Window(const char* title, int width, int height);
    ~Window();

    bool initialize();
    void toggleFullscreen();
    void togglePause();
    void handleResize(int newWidth, int newHeight);
    void swapBuffers();

    SDL_Window* getSDLWindow() const { return window; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    bool isFullscreen() const { return fullscreen; }
    bool isPaused() const { return paused; }

private:
    SDL_Window* window;
    SDL_GLContext glContext;
    int width;
    int height;
    bool fullscreen;
    bool paused;
    std::string title;
};

#endif