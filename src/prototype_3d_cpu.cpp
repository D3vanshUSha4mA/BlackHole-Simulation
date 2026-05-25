#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "Physics.h" // Our decoupled math module!
#include <vector>
#include <iostream>
#include <chrono>

using namespace std;
using namespace glm;

const int WIDTH = 100;
const int HEIGHT = 75;
const int MAX_STEPS = 600;
const float DT = 0.05f;

// --- SIMPLE TEXTURE SHADERS ---
const char* vsSrc = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;
out vec2 TexCoords;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoords = aTexCoords;
}
)";

const char* fsSrc = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D screenTexture;
void main() {
    FragColor = texture(screenTexture, TexCoords);
}
)";

GLuint createShader() {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, nullptr);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, nullptr);
    glCompileShader(fs);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    return program;
}

// --- THE INTENTIONAL CPU BOTTLENECK ---
void calculateFrame(vector<uint8_t>& pixels) {
    vec3 camPos(0.0f, 2.0f, 8.0f);
    vec3 target(0.0f, 0.0f, 0.0f);
    vec3 forward = normalize(target - camPos);
    vec3 right = normalize(cross(vec3(0.0f, 1.0f, 0.0f), forward));
    vec3 up = cross(forward, right);

    // Loop over every single pixel on the CPU (This will lag intensely)
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            
            vec2 uv((float)x / WIDTH, (float)y / HEIGHT);
            uv = uv * 2.0f - 1.0f;
            uv.x *= (float)WIDTH / HEIGHT;

            vec3 rayDir = normalize(uv.x * right + uv.y * up + 1.5f * forward);
            vec3 pos = camPos;
            vec3 vel = rayDir;
            
            bool absorbed = false;
            vec3 color(0.01f, 0.01f, 0.03f); // Background space color

            // RK4 Raymarching
            for (int i = 0; i < MAX_STEPS; i++) {
                vec3 old_pos = pos;
                
                // Pulling directly from our modular Physics engine!
                Physics::stepRK4(pos, vel, DT);

                // Simple accretion disk check
                if (old_pos.y * pos.y < 0.0f) {
                    float r_disk = length(vec2(pos.x, pos.z));
                    if (r_disk > 2.6f * Physics::RS && r_disk < 8.0f * Physics::RS) {
                        color = vec3(0.9f, 0.4f, 0.1f); // Orange disk
                        break;
                    }
                }

                // Event Horizon check
                if (length(pos) < Physics::RS) {
                    absorbed = true;
                    break;
                }
            }

            if (absorbed) color = vec3(0.0f);

            // Write to pixel buffer
            int index = (y * WIDTH + x) * 3;
            pixels[index] = (uint8_t)(std::min(color.r, 1.0f) * 255.0f);
            pixels[index + 1] = (uint8_t)(std::min(color.g, 1.0f) * 255.0f);
            pixels[index + 2] = (uint8_t)(std::min(color.b, 1.0f) * 255.0f);
        }
    }
}

int main() {
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "CPU Bottleneck Test", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    GLuint shader = createShader();

    // Quad setup
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // CPU Pixel Buffer & GPU Texture
    vector<uint8_t> pixels(WIDTH * HEIGHT * 3, 0);
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    while (!glfwWindowShouldClose(window)) {
        
        auto start = chrono::high_resolution_clock::now();
        
        // 1. Calculate the entire frame on the CPU
        calculateFrame(pixels);
        
        // 2. Send the CPU buffer to the GPU (Very slow!)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WIDTH, HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

        // 3. Draw the texture to the screen
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glfwSwapBuffers(window);
        glfwPollEvents();

        auto end = chrono::high_resolution_clock::now();
        chrono::duration<float> duration = end - start;
        cout << "CPU Frame Time: " << duration.count() << " seconds (" << 1.0f / duration.count() << " FPS)" << endl;
    }

    return 0;
}