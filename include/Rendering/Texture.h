#ifndef TEXTURE_H
#define TEXTURE_H

class Texture {
public:
    Texture(const char* path);
    ~Texture();

    void bind();
    unsigned int getID() const { return textureID; }

private:
    unsigned int textureID;
};

#endif