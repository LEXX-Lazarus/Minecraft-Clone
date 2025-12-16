#ifndef SHADER_H
#define SHADER_H

class Shader {
public:
    Shader(const char* vertexSource, const char* fragmentSource);
    ~Shader();

    void use();
    unsigned int getID() const { return programID; }

private:
    unsigned int programID;
    unsigned int compileShader(unsigned int type, const char* source);
};

#endif