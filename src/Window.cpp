#include "Window.h"
#include <glad/glad.h>
#include <iostream>

Window::Window(const char* title, int width, int height)
    : window(nullptr), glContext(nullptr), width(width), height(height),
    fullscreen(false), paused(false), title(title) {
}

Window::~Window() {
    if (glContext) {
        SDL_GL_DestroyContext(glContext);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
}

bool Window::initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Enable double buffering and depth buffer for better performance
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    window = SDL_CreateWindow(
        title.c_str(),
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        return false;
    }

    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        return false;
    }

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    SDL_GL_SetSwapInterval(0); // VSync OFF (uncapped FPS for profiling)

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    return true;
}

void Window::toggleFullscreen() {
    // Cache mouse state to avoid multiple SDL calls
    float mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    // Calculate normalized position using cached width/height
    const float invWidth = 1.0f / static_cast<float>(width);
    const float invHeight = 1.0f / static_cast<float>(height);
    const float normalizedX = mouseX * invWidth;
    const float normalizedY = mouseY * invHeight;

    fullscreen = !fullscreen;

    // Single SDL call for fullscreen toggle
    SDL_SetWindowFullscreen(window, fullscreen);

    // Get new dimensions after fullscreen change
    SDL_GetWindowSize(window, &width, &height);

    // Warp mouse to same normalized position in new coordinate system
    SDL_WarpMouseInWindow(window,
        static_cast<float>(normalizedX * width),
        static_cast<float>(normalizedY * height));
}

void Window::togglePause() {
    paused = !paused;
    SDL_SetWindowRelativeMouseMode(window, !paused);
}

void Window::handleResize(int newWidth, int newHeight) {
    width = newWidth;
    height = newHeight;
    glViewport(0, 0, width, height);
}

void Window::swapBuffers() {
    SDL_GL_SwapWindow(window);
}