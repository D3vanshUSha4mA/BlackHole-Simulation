// ================== OPENGL LOADER ==================
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// ================== GLM ==================
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ================== STL ==================
#include <vector>
#include <iostream>
#include <cmath>
#include <chrono>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace glm;
using namespace std;
using Clock = std::chrono::high_resolution_clock;

// ==================================================
// GLOBALS
// ==================================================
double c = 299792458.0;
double G = 6.67430e-11;
bool useGeodesics = false;

double lastPrintTime = 0.0;
int framesCount = 0;

// ==================================================
// CAMERA
// ==================================================
struct Camera {
    vec3 pos{0};
    vec3 target{0};
    float fovY = 60.0f;
    float azimuth = 0.0f, elevation = M_PI / 2.0f;
    float radius = 6.3e10f;
    float minRadius = 1e12f, maxRadius = 1e20f;

    bool dragging = false, panning = false;
    double lastX = 0, lastY = 0;

    float orbitSpeed = 0.008f;
    float panSpeed   = 0.001f;
    float zoomSpeed  = 1.08f;

    Camera() { updateVectors(); }

    void updateVectors() {
        pos.x = target.x + radius * sin(elevation) * cos(azimuth);
        pos.y = target.y + radius * cos(elevation);
        pos.z = target.z + radius * sin(elevation) * sin(azimuth);
    }

    void processMouse(double x, double y) {
        float dx = float(x - lastX);
        float dy = float(y - lastY);

        if (dragging && !panning) {
            azimuth   -= dx * orbitSpeed;
            elevation -= dy * orbitSpeed;
            elevation = glm::clamp(elevation, 0.01f, float(M_PI) - 0.01f);
        } else if (panning) {
            vec3 f = normalize(target - pos);
            vec3 r = normalize(cross(f, vec3(0,1,0)));
            vec3 u = cross(r, f);
            target += (-r * dx + u * dy) * panSpeed * radius;
        }
        updateVectors();
        lastX = x; lastY = y;
    }

    void processScroll(double y) {
        radius *= (y < 0) ? pow(zoomSpeed, -y) : 1.0 / pow(zoomSpeed, y);
        radius    = glm::clamp(radius, minRadius, maxRadius);
        updateVectors();
    }

    static void mouseButton(GLFWwindow* w, int b, int a, int m) {
        auto* c = (Camera*)glfwGetWindowUserPointer(w);
        if (b == GLFW_MOUSE_BUTTON_LEFT) {
            if (a == GLFW_PRESS) {
                c->dragging = true;
                c->panning = (m & GLFW_MOD_SHIFT);
                glfwGetCursorPos(w, &c->lastX, &c->lastY);
            } else {
                c->dragging = c->panning = false;
            }
        }
    }

    static void cursor(GLFWwindow* w, double x, double y) {
        ((Camera*)glfwGetWindowUserPointer(w))->processMouse(x, y);
    }

    static void scroll(GLFWwindow* w, double, double y) {
        ((Camera*)glfwGetWindowUserPointer(w))->processScroll(y);
    }
};
Camera camera;

// ==================================================
// ENGINE (MODERN OPENGL, NO GLEW)
// ==================================================
struct Engine {
    GLFWwindow* window{};
    GLuint quadVAO{}, quadVBO{}, texture{}, shader{};
    int WIDTH = 800, HEIGHT = 600;

    Engine() {
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Black Hole", nullptr, nullptr);
        glfwMakeContextCurrent(window);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            cerr << "GLAD init failed\n";
            exit(1);
        }

        cout << "OpenGL " << glGetString(GL_VERSION) << "\n";
        shader = createShader();
        createQuad();
    }

    GLuint createShader() {
        const char* vs = R"(#version 330 core
        layout(location=0) in vec2 p;
        layout(location=1) in vec2 t;
        out vec2 uv;
        void main(){ uv=t; gl_Position=vec4(p,0,1); })";

        const char* fs = R"(#version 330 core
        in vec2 uv; out vec4 c;
        uniform sampler2D tex;
        void main(){ c=texture(tex,uv); })";

        auto compile = [](GLenum t, const char* s) {
            GLuint sh = glCreateShader(t);
            glShaderSource(sh,1,&s,nullptr);
            glCompileShader(sh);
            return sh;
        };

        GLuint p = glCreateProgram();
        GLuint v = compile(GL_VERTEX_SHADER,vs);
        GLuint f = compile(GL_FRAGMENT_SHADER,fs);
        glAttachShader(p,v); glAttachShader(p,f);
        glLinkProgram(p);
        glDeleteShader(v); glDeleteShader(f);
        return p;
    }

    void createQuad() {
        float q[]={
            -1,1,0,1, -1,-1,0,0, 1,-1,1,0,
            -1,1,0,1, 1,-1,1,0, 1,1,1,1
        };

        glGenVertexArrays(1,&quadVAO);
        glGenBuffers(1,&quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER,quadVBO);
        glBufferData(GL_ARRAY_BUFFER,sizeof(q),q,GL_STATIC_DRAW);

        glVertexAttribPointer(0,2,GL_FLOAT,0,4*sizeof(float),(void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,2,GL_FLOAT,0,4*sizeof(float),(void*)(2*sizeof(float)));
        glEnableVertexAttribArray(1);

        glGenTextures(1,&texture);
        glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    }

    void render(const vector<unsigned char>& px) {
        glBindTexture(GL_TEXTURE_2D,texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,WIDTH,HEIGHT,0,GL_RGB,GL_UNSIGNED_BYTE,px.data());
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES,0,6);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
};
Engine engine;

// ==================================================
// CALLBACKS
// ==================================================
void setupCallbacks() {
    glfwSetWindowUserPointer(engine.window, &camera);
    glfwSetMouseButtonCallback(engine.window, Camera::mouseButton);
    glfwSetCursorPosCallback(engine.window, Camera::cursor);
    glfwSetScrollCallback(engine.window, Camera::scroll);
    glfwSetKeyCallback(engine.window, [](GLFWwindow*,int k,int, int a,int){
        if(a==GLFW_PRESS && k==GLFW_KEY_G){
            useGeodesics=!useGeodesics;
            cout<<"Geodesics "<<(useGeodesics?"ON\n":"OFF\n");
        }
    });
}

// ==================================================
// MAIN
// ==================================================
int main() {
    setupCallbacks();
    vector<unsigned char> pixels(engine.WIDTH*engine.HEIGHT*3);

    while(!glfwWindowShouldClose(engine.window)) {
        // raytrace(pixels)  // your existing raytrace()
        engine.render(pixels);

        framesCount++;
        double now = chrono::duration<double>(Clock::now().time_since_epoch()).count();
        if (now-lastPrintTime>1.0) {
            cout<<"FPS "<<framesCount<<"\n";
            framesCount=0; lastPrintTime=now;
        }
    }

    glfwTerminate();
    return 0;
}
