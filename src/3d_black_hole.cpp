#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
using namespace std;


const char* vertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* fragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;

uniform vec2 uResolution;
uniform vec3 uCamPos;

const float rs = 1.0;       
const float dt = 0.05;      
const int MAX_STEPS = 600; 

// 3D Geodesic Equation
vec3 getAcceleration(vec3 pos, vec3 vel) {
    float r2 = dot(pos, pos); 
    float r = sqrt(r2);       
    float r5 = r2 * r2 * r;   
    
    vec3 L = cross(pos, vel);
    float L2 = dot(L, L);     
    
    return -1.5 * rs * L2 / r5 * pos;
}

// --- NEW: Function to define the curved grid surface ---
// Returns the height (y) of the grid at a given horizontal position (pos.xz)
float getGridHeight(vec3 p) {
    // The scale determines how deep the "gravity well" is.
    float curvatureScale = 1.5 * rs; 
    // Avoid division by zero near the center
    float r_xz = max(length(p.xz), 0.1); 
    // The negative sign makes it dip downwards
    return -curvatureScale / r_xz;
}

void main() {
    vec2 uv = gl_FragCoord.xy / uResolution.xy;
    uv = uv * 2.0 - 1.0;
    uv.x *= uResolution.x / uResolution.y;

    vec3 ro = uCamPos;
    vec3 target = vec3(0.0, 0.0, 0.0);
    
    vec3 forward = normalize(target - ro);
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
    vec3 up = cross(forward, right);
    
    // A Field of View of 1.5 gives a nice wide look
    vec3 rd = normalize(uv.x * right + uv.y * up + 1.5 * forward); 

    vec3 pos = ro;
    vec3 vel = rd; 

    bool absorbed = false;
    vec3 finalColor = vec3(0.0); 

    float diskInner = 2.6 * rs; 
    float diskOuter = 8.0 * rs;
    vec3 gridColor = vec3(0.8, 0.3, 0.2); // Reddish-orange for the grid
    float gridSize = 1.0; 

    for (int i = 0; i < MAX_STEPS; i++) {
        vec3 old_pos = pos; 
        
        // --- RK4 Integration Step ---
        vec3 k1_v = getAcceleration(pos, vel);
        vec3 k1_p = vel;
        vec3 k2_v = getAcceleration(pos + k1_p * dt * 0.5, vel + k1_v * dt * 0.5);
        vec3 k2_p = vel + k1_v * dt * 0.5;
        vec3 k3_v = getAcceleration(pos + k2_p * dt * 0.5, vel + k2_v * dt * 0.5);
        vec3 k3_p = vel + k2_v * dt * 0.5;
        vec3 k4_v = getAcceleration(pos + k3_p * dt, vel + k3_v * dt);
        vec3 k4_p = vel + k3_v * dt;
        
        pos += (dt / 6.0) * (k1_p + 2.0*k2_p + 2.0*k3_p + k4_p);
        vel += (dt / 6.0) * (k1_v + 2.0*k2_v + 2.0*k3_v + k4_v);

        // --- DISK INTERSECTION (Flat plane at y=0) ---
        // We keep the accretion disk flat for contrast, like in the reference image.
        if (old_pos.y * pos.y < 0.0) {
            float t_disk = -old_pos.y / (pos.y - old_pos.y);
            vec3 intersectPos = mix(old_pos, pos, t_disk);
            float r_disk = length(intersectPos.xz); 

            if (r_disk > diskInner && r_disk < diskOuter) {
                float temp = 1.0 - smoothstep(diskInner, diskOuter, r_disk);
                vec3 glowColor = mix(vec3(0.8, 0.2, 0.0), vec3(1.0, 0.9, 0.6), temp);
                finalColor += glowColor * temp * 2.0; // Bright glowing disk
            }
        }

        // --- NEW: CURVED GRID INTERSECTION ---
        // Calculate the "height difference" from the curved surface
        float h_old = old_pos.y - getGridHeight(old_pos);
        float h_new = pos.y - getGridHeight(pos);

        // If the signs are different, the ray has crossed the curved surface
        if (h_old * h_new < 0.0) {
            // Find the approximate intersection point using interpolation
            float t_grid = h_old / (h_old - h_new);
            vec3 gridIntersect = mix(old_pos, pos, t_grid);
            float r_grid = length(gridIntersect.xz);

            // Only draw grid outside the event horizon
            if (r_grid > rs * 1.1) { 
                vec2 gridPattern = fract(gridIntersect.xz / gridSize);
                float lineThickness = 0.03; // Slightly thicker lines
                float gridX = 1.0 - smoothstep(lineThickness, lineThickness + 0.02, abs(gridPattern.x - 0.5) * 2.0);
                float gridZ = 1.0 - smoothstep(lineThickness, lineThickness + 0.02, abs(gridPattern.y - 0.5) * 2.0);
                float grid = max(gridX, gridZ);
                
                // Fade out at a distance
                float gridFade = 1.0 - smoothstep(10.0, 30.0, r_grid);
                
                finalColor += gridColor * grid * gridFade * 0.5;
            }
        }

        if (length(pos) < rs) {
            absorbed = true;
            break; 
        }
        if (length(pos) > 35.0) break; 
    }

    if (absorbed) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0); 
    } else {
        // Faint background space color
        FragColor = vec4(finalColor + vec3(0.01, 0.01, 0.03), 1.0); 
    }
}
)";

// --- HELPERS ---
GLuint createProgram() {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSrc, nullptr);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSrc, nullptr);
    glCompileShader(fs);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// --- ENGINE ---
// --- ENGINE ---
struct Engine {
    GLFWwindow* window;
    GLuint shader;
    GLuint vao, vbo;

    // --- Camera Variables ---
    float camYaw = 0.0f;     // Left/Right angle
    float camPitch = 0.2f;   // Up/Down angle
    float camRadius = 8.0f;  // Distance from black hole
    double lastMouseX = 0.0, lastMouseY = 0.0;
    bool firstMouse = true;

    bool init() {
        if (!glfwInit()) return false;
        window = glfwCreateWindow(800, 600, "3D Black Hole Raytracer", nullptr, nullptr);
        glfwMakeContextCurrent(window);
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return false;

        shader = createProgram();

        float quadVertices[] = {
            -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f   
        };

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        return true;
    }

    // --- NEW: Handle Input ---
    void processInput() {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        // If holding Left Click, rotate the camera
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            if (firstMouse) {
                lastMouseX = mouseX;
                lastMouseY = mouseY;
                firstMouse = false;
            }

            float dx = (float)(mouseX - lastMouseX);
            float dy = (float)(mouseY - lastMouseY);

            camYaw += dx * 0.01f;
            camPitch += dy * 0.01f;

            // Clamp pitch so we don't flip upside down
            if (camPitch > 1.5f) camPitch = 1.5f;
            if (camPitch < -1.5f) camPitch = -1.5f;
        } else {
            firstMouse = true; // Reset when you let go of the click
        }

        lastMouseX = mouseX;
        lastMouseY = mouseY;
    }

    void render() {
        processInput(); // Check for mouse movement first

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader);

        // --- Calculate Spherical Camera Position ---
        // Converts Angles (Yaw/Pitch) into 3D XYZ coordinates
        float camX = camRadius * cos(camPitch) * sin(camYaw);
        float camY = camRadius * sin(camPitch);
        float camZ = camRadius * cos(camPitch) * cos(camYaw);

        // Send variables to the GPU
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glUniform2f(glGetUniformLocation(shader, "uResolution"), (float)width, (float)height);
        glUniform3f(glGetUniformLocation(shader, "uCamPos"), camX, camY, camZ);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
};
int main() {
    Engine engine;
    if (engine.init()) {
        while (!glfwWindowShouldClose(engine.window)) {
            engine.render();
            glfwSwapBuffers(engine.window);
            glfwPollEvents();
        }
    }
    return 0;
}