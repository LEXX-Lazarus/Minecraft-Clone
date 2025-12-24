#include "Rendering/Skybox.h"
#include <gtc/type_ptr.hpp>
#include <random>
#include <cmath>
#include <iostream>

// STB_IMAGE_IMPLEMENTATION is defined in main.cpp
#include "stb_image.h"

Skybox::Skybox()
    : starVAO(0), starVBO(0), starShaderProgram(0)
{
    // Initialize with textureID 0 (will be set in initialize)
    // INCREASED DISTANCE to 800.0f so they are further away
    sun = { 0, 0, 0, 0, 50.0f, 225.0f };
    moon = { 0, 0, 0, 0, 40.0f, 350.0f };
}

Skybox::~Skybox() {
    if (starVAO) glDeleteVertexArrays(1, &starVAO);
    if (starVBO) glDeleteBuffers(1, &starVBO);
    if (starShaderProgram) glDeleteProgram(starShaderProgram);

    if (sun.VAO) glDeleteVertexArrays(1, &sun.VAO);
    if (sun.VBO) glDeleteBuffers(1, &sun.VBO);
    if (sun.shaderProgram) glDeleteProgram(sun.shaderProgram);
    if (sun.textureID) glDeleteTextures(1, &sun.textureID);

    if (moon.VAO) glDeleteVertexArrays(1, &moon.VAO);
    if (moon.VBO) glDeleteBuffers(1, &moon.VBO);
    if (moon.shaderProgram) glDeleteProgram(moon.shaderProgram);
    if (moon.textureID) glDeleteTextures(1, &moon.textureID);
}

void Skybox::initialize() {
    generateStars();
    createStarShader();
    createSunMoonShader();

    // Load the textures
    sun.textureID = loadTexture("assets/textures/skybox/Sun.png");
    moon.textureID = loadTexture("assets/textures/skybox/FullMoon.png");
}

unsigned int Skybox::loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set texture wrapping parameters (clamp to edge prevents white lines)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Load image
    int width, height, nrChannels;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = GL_RGB;
        if (nrChannels == 4) format = GL_RGBA; // Handle transparency in PNGs

        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        std::cout << "Loaded Skybox texture: " << path << std::endl;
    }
    else {
        std::cerr << "Failed to load skybox texture: " << path << std::endl;
    }

    stbi_image_free(data);
    return textureID;
}

/////////////////////////////////////
// STAR FUNCTIONS
/////////////////////////////////////
void Skybox::generateStars() {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> brightDist(0.3f, 1.0f);

    for (int i = 0; i < 3000; i++) {
        glm::vec3 pos;
        do { pos = glm::vec3(dist(rng), dist(rng), dist(rng)); } while (glm::length(pos) > 1.0f);

        pos = glm::normalize(pos) * 800.0f;
        if (pos.y > -500.0f) {
            Star star;
            star.position = pos;
            star.brightness = brightDist(rng);
            stars.push_back(star);
        }
    }

    std::vector<float> vertexData;
    for (const auto& star : stars) {
        vertexData.push_back(star.position.x);
        vertexData.push_back(star.position.y);
        vertexData.push_back(star.position.z);
        vertexData.push_back(star.brightness);
    }

    glGenVertexArrays(1, &starVAO);
    glGenBuffers(1, &starVBO);

    glBindVertexArray(starVAO);
    glBindBuffer(GL_ARRAY_BUFFER, starVBO);
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Skybox::createStarShader() {
    const char* vertexShaderSource = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in float aBrightness;
        out float brightness;
        uniform mat4 view;
        uniform mat4 projection;
        void main() {
            brightness = aBrightness;
            gl_Position = projection * view * vec4(aPos, 1.0);
            gl_PointSize = 6.0;
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 330 core
        out vec4 FragColor;
        in float brightness;
        uniform float starVisibility;
        void main() {
            vec2 coord = gl_PointCoord - vec2(0.5);
            float dist = length(coord);
            if (dist > 0.5) discard;
            float alpha = (1.0 - dist * 2.0) * brightness * starVisibility;
            FragColor = vec4(1.0, 1.0, 1.0, alpha);
        }
    )";

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    starShaderProgram = glCreateProgram();
    glAttachShader(starShaderProgram, vertexShader);
    glAttachShader(starShaderProgram, fragmentShader);
    glLinkProgram(starShaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

/////////////////////////////////////
// SUN & MOON FUNCTIONS
/////////////////////////////////////
void Skybox::createSunMoonShader() {
    const char* vertexShaderSource = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        out vec2 TexCoord;
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        void main() {
            TexCoord = aPos.xy + 0.5;
            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D uTexture;
        void main() {
            vec4 texColor = texture(uTexture, TexCoord);
            if(texColor.a < 0.1) discard;
            FragColor = texColor;
        }
    )";

    auto compileShaderProgram = [&](CelestialBody& body) {
        unsigned int vShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vShader, 1, &vertexShaderSource, nullptr);
        glCompileShader(vShader);

        unsigned int fShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fShader, 1, &fragmentShaderSource, nullptr);
        glCompileShader(fShader);

        body.shaderProgram = glCreateProgram();
        glAttachShader(body.shaderProgram, vShader);
        glAttachShader(body.shaderProgram, fShader);
        glLinkProgram(body.shaderProgram);

        glDeleteShader(vShader);
        glDeleteShader(fShader);

        float quad[] = {
            -0.5f, -0.5f, 0.0f,
            -0.5f,  0.5f, 0.0f,
             0.5f,  0.5f, 0.0f,
             0.5f, -0.5f, 0.0f
        };
        glGenVertexArrays(1, &body.VAO);
        glGenBuffers(1, &body.VBO);
        glBindVertexArray(body.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, body.VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
        };

    compileShaderProgram(sun);
    compileShaderProgram(moon);
}

glm::vec3 Skybox::getCelestialPosition(float timeOfDay, bool isSun) {
    const float PI = 3.14159265f;
    // timeOfDay 0.0 = Midnight, 0.25 = Sunrise, 0.5 = Noon
    float angle = (timeOfDay * 2.0f * PI) - (PI / 2.0f);

    if (!isSun) {
        angle += PI;
    }

    float distance = isSun ? sun.distance : moon.distance;

    // REMOVED the 0.3f Z-bias. 
    // Now the sun/moon orbit strictly on the X/Y plane (East-West).
    // This centers them in the sky when looking forward.
    glm::vec3 direction = glm::vec3(
        std::cos(angle),
        std::sin(angle),
        0.0f
    );

    return direction * distance;
}

void Skybox::renderCelestial(const float* viewMatrix, const float* projectionMatrix, float timeOfDay) {
    glm::mat4 view = glm::make_mat4(viewMatrix);
    view[3] = glm::vec4(0, 0, 0, 1);
    glm::mat4 proj = glm::make_mat4(projectionMatrix);

    auto renderBody = [&](CelestialBody& body, bool isSun) {
        glUseProgram(body.shaderProgram);
        glBindVertexArray(body.VAO);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, body.textureID);
        glUniform1i(glGetUniformLocation(body.shaderProgram, "uTexture"), 0);

        glm::vec3 pos = getCelestialPosition(timeOfDay, isSun);

        // Billboard logic
        glm::mat4 viewRotation = view;
        viewRotation[3] = glm::vec4(0, 0, 0, 1);
        glm::mat4 billboardRot = glm::transpose(viewRotation);

        glm::mat4 model = glm::translate(glm::mat4(1.0f), pos)
            * billboardRot
            * glm::scale(glm::mat4(1.0f), glm::vec3(body.size));

        glUniformMatrix4fv(glGetUniformLocation(body.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(body.shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(body.shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(proj));

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glBindVertexArray(0);
        };

    renderBody(sun, true);
    renderBody(moon, false);
}

/////////////////////////////////////
// MAIN RENDER
/////////////////////////////////////
void Skybox::render(const float* viewMatrix, const float* projectionMatrix, float timeOfDay) {
    // Stars
    float hours = timeOfDay * 24.0f;
    float starVisibility = 0.0f;

    if (hours >= 18.0f || hours < 6.0f) {
        if (hours >= 18.0f) starVisibility = glm::smoothstep(18.0f, 21.0f, hours);
        else starVisibility = glm::smoothstep(6.0f, 5.0f, hours);
    }

    if (starVisibility > 0.01f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glEnable(GL_PROGRAM_POINT_SIZE);
        glDepthMask(GL_FALSE);

        glUseProgram(starShaderProgram);
        glm::mat4 view = glm::make_mat4(viewMatrix);
        view[3] = glm::vec4(0, 0, 0, 1);
        glm::mat4 proj = glm::make_mat4(projectionMatrix);

        glUniformMatrix4fv(glGetUniformLocation(starShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(starShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniform1f(glGetUniformLocation(starShaderProgram, "starVisibility"), starVisibility);

        glBindVertexArray(starVAO);
        glDrawArrays(GL_POINTS, 0, stars.size());
        glBindVertexArray(0);

        glDepthMask(GL_TRUE);
        glDisable(GL_PROGRAM_POINT_SIZE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // Sun & Moon
    glDisable(GL_CULL_FACE);
    renderCelestial(viewMatrix, projectionMatrix, timeOfDay);
    glEnable(GL_CULL_FACE);
}