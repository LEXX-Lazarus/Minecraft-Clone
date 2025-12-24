#include "Rendering/Skybox.h"
#include <gtc/type_ptr.hpp>
#include <random>
#include <cmath>

Skybox::Skybox() : starVAO(0), starVBO(0), starShaderProgram(0) {}

Skybox::~Skybox() {
    if (starVAO) glDeleteVertexArrays(1, &starVAO);
    if (starVBO) glDeleteBuffers(1, &starVBO);
    if (starShaderProgram) glDeleteProgram(starShaderProgram);
}

void Skybox::initialize() {
    generateStars();
    createStarShader();
}

void Skybox::generateStars() {
    std::mt19937 rng(42);  // Fixed seed for consistent stars
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> brightDist(0.3f, 1.0f);

    // Generate 3000 stars randomly distributed in a sphere
    for (int i = 0; i < 3000; i++) {
        glm::vec3 pos;
        do {
            pos = glm::vec3(dist(rng), dist(rng), dist(rng));
        } while (glm::length(pos) > 1.0f);  // Keep stars inside unit sphere

        pos = glm::normalize(pos) * 800.0f;  // Push to far distance

        // Don't place stars too low (below horizon)
        if (pos.y > -500.0f) {
            Star star;
            star.position = pos;
            star.brightness = brightDist(rng);
            stars.push_back(star);
        }
    }

    // Create vertex buffer with star positions and brightness
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

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Brightness attribute
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Skybox::createStarShader() {
    const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in float aBrightness;
    
    out float brightness;
    
    uniform mat4 view;
    uniform mat4 projection;
    
    void main() {
        brightness = aBrightness;
        gl_Position = projection * view * vec4(aPos, 1.0);
        gl_PointSize = 6.0;  // Star size in pixels
    }
    )";

    const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    
    in float brightness;
    
    uniform float starVisibility;  // 0.0 = invisible (day), 1.0 = visible (night)
    
    void main() {
        // Make stars round instead of square
        vec2 coord = gl_PointCoord - vec2(0.5);
        float dist = length(coord);
        if (dist > 0.5) discard;
        
        float alpha = (1.0 - dist * 2.0) * brightness * starVisibility;
        FragColor = vec4(1.0, 1.0, 1.0, alpha);
    }
    )";

    // Compile vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    // Compile fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    // Link shader program
    starShaderProgram = glCreateProgram();
    glAttachShader(starShaderProgram, vertexShader);
    glAttachShader(starShaderProgram, fragmentShader);
    glLinkProgram(starShaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void Skybox::render(const float* viewMatrix, const float* projectionMatrix, float timeOfDay) {
    // Calculate star visibility based on time of day
    // Stars visible at night (roughly 8 PM to 6 AM)
    float hours = timeOfDay * 24.0f;
    float starVisibility = 0.0f;

    if (hours >= 18.0f || hours < 6.0f) {
        // Night time - stars visible
        if (hours >= 18.0f) {
            // 8 PM - midnight: fade in
            starVisibility = glm::smoothstep(18.0f, 21.0f, hours);
        }
        else {
            // Midnight - 6 AM: fade out
            starVisibility = glm::smoothstep(6.0f, 5.0f, hours);
        }
    }

    if (starVisibility < 0.01f) return;  // Don't render if invisible

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending for stars
    glEnable(GL_PROGRAM_POINT_SIZE);
    glDepthMask(GL_FALSE);

    glm::mat4 view = glm::make_mat4(viewMatrix);
    view[3] = glm::vec4(0, 0, 0, 1);  // Remove translation
    glm::mat4 proj = glm::make_mat4(projectionMatrix);

    glUseProgram(starShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(starShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(starShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform1f(glGetUniformLocation(starShaderProgram, "starVisibility"), starVisibility);

    glBindVertexArray(starVAO);
    glDrawArrays(GL_POINTS, 0, stars.size());
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // Reset blend mode
}