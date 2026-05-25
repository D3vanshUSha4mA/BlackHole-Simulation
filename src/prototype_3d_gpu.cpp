#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

// ImGui Headers
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

// Forces High-Performance GPU
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN // Prevents Windows from including conflicting legacy libraries
#include <windows.h>
extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

// --- UTILITY: Load Shaders ---
std::string loadShaderSource(const char* filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint compileShader(const char* vertexPath, const char* fragmentPath) {
    std::string vsStr = loadShaderSource(vertexPath);
    std::string fsStr = loadShaderSource(fragmentPath);
    const char* vsSrc = vsStr.c_str();
    const char* fsSrc = fsStr.c_str();

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

// --- ENGINE STATE ---
float camYaw = 0.0f;
float camPitch = 0.2f;
float camRadius = 8.0f;
double lastMouseX = 0.0, lastMouseY = 0.0;
bool isDragging = false;

// --- INTERACTIVE UI VARIABLES ---
float ui_Mass = 1.0f;
float ui_Exposure = 0.8f;
float ui_DiskSpread = 8.0f;

void processInput(GLFWwindow* window) {
    // Only move camera if ImGui isn't capturing the mouse (like when dragging a slider)
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        if (!isDragging) { lastMouseX = mouseX; lastMouseY = mouseY; isDragging = true; }
        camYaw += (float)(mouseX - lastMouseX) * 0.01f;
        camPitch += (float)(mouseY - lastMouseY) * 0.01f;
        if (camPitch > 1.5f) camPitch = 1.5f;
        if (camPitch < -1.5f) camPitch = -1.5f;
    } else {
        isDragging = false;
    }
    lastMouseX = mouseX;
    lastMouseY = mouseY;
}

int main() {
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(1024, 768, "Black Hole Rendering Engine", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    GLuint shader = compileShader("../src/shader.vert", "../src/shader.frag");

    float quadVertices[] = { -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f };
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        processInput(window);

        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- UI WINDOW OVERLAY ---
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::Begin("Engine Controls");
        ImGui::Text("Application Average: %.1f FPS", ImGui::GetIO().Framerate);
        ImGui::Separator();
        ImGui::SliderFloat("Singularity Mass", &ui_Mass, 0.1f, 3.0f);
        ImGui::SliderFloat("Disk Spread", &ui_DiskSpread, 4.0f, 15.0f);
        ImGui::SliderFloat("Camera Exposure", &ui_Exposure, 0.1f, 3.0f);
        ImGui::End();

        // Render Graphics
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader);

        float camX = camRadius * cos(camPitch) * sin(camYaw);
        float camY = camRadius * sin(camPitch);
        float camZ = camRadius * cos(camPitch) * cos(camYaw);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        
        // Send Uniforms to GPU
        glUniform2f(glGetUniformLocation(shader, "uResolution"), (float)width, (float)height);
        glUniform3f(glGetUniformLocation(shader, "uCamPos"), camX, camY, camZ);
        glUniform1f(glGetUniformLocation(shader, "uMass"), ui_Mass);
        glUniform1f(glGetUniformLocation(shader, "uExposure"), ui_Exposure);
        glUniform1f(glGetUniformLocation(shader, "uDiskSpread"), ui_DiskSpread);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Render ImGui over the graphics
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}