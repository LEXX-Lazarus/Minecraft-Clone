#ifndef RENDERER_H
#define RENDERER_H

class Renderer {
public:
    Renderer();
    ~Renderer();

    void render();

private:
    unsigned int VAO, VBO, EBO;
    void setupMesh();
};

#endif