#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <cmath>

using namespace std;
using namespace glm;

// --- CONSTANTS ---
const float WORLD_WIDTH = 1.0e11f;
const float WORLD_HEIGHT = 7.5e10f;
constexpr float PI = 3.14159265358979323846f;
constexpr double G = 6.67430e-11;
constexpr double c = 299792458.0;

// --- SHADERS ---
const char* vertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in float aAlpha; 
out float vAlpha; 
uniform mat4 uProjection;
void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    vAlpha = aAlpha;
}
)";

const char* fragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;
in float vAlpha;
uniform vec3 uColor;
void main() {
    FragColor = vec4(uColor, vAlpha); 
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

struct Blackhole {
    vec2 position;
    double mass;
    float r_s;
    GLuint vao = 0, vbo = 0;
    int vertexCount = 0;

    Blackhole(vec2 pos,double m):position(pos),mass(m){
        r_s=static_cast<float>((2.0*G*mass)/(c*c));        //The most imp property of the black hole(r_s defines the event horizon,any light ray that crosses this distance can never escape)
        buildMesh();               //creates a circle of vertices,uses sin and cos functions to generate 100 points around the center at radius r_s
    }

    void buildMesh() {
        vector<vec2>vertices;
        vertices.emplace_back(0.0f,0.0f);
        int segments=100;
        for (int i=0;i<=segments;++i){
            float angle=2.0f*PI*i/segments;
            vertices.emplace_back(cos(angle)*r_s,sin(angle)*r_s);
        }
        vertexCount=(int)vertices.size();

        glGenVertexArrays(1,&vao);
        glGenBuffers(1,&vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(vec2),vertices.data(),GL_STATIC_DRAW);
        glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(vec2),(void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    void draw(GLuint shader) {
        mat4 model = translate(mat4(1.0f), vec3(position, 0.0f));
        glUseProgram(shader);
        glUniformMatrix4fv(glGetUniformLocation(shader, "uModel"), 1, GL_FALSE, value_ptr(model));
        glUniform3f(glGetUniformLocation(shader, "uColor"), 1.0f, 0.0f, 0.0f); 
        glVertexAttrib1f(1, 1.0f);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, vertexCount);
        glBindVertexArray(0);
    }
};

struct TrailPoint {
    vec2 pos;
    float alpha;
};

struct Ray {
    
    double r;       // Distance from center
    double phi;     // Angle
    double dr;      // Radial Velocity
    double dphi;    // Angular Velocity
    double E;       // Energy

    
    vec2 pos;       // Current Position
    vec2 dir;       // Current Direction 

    vector<vec2> positions; //memory buffer acts storing the last 3000 locations of the photons
    const size_t MAX_TRAIL_LENGTH = 3000; 
    GLuint vao = 0;
    GLuint vbo = 0;
    bool absorbed = false;

    Ray(vec2 startPos, vec2 direction, double rs) {
        pos=startPos;
        dir=normalize(direction); 
        double x=startPos.x;
        double y=startPos.y;
        r=sqrt(x*x+y*y);
        phi=atan2(y,x);
        double vx=direction.x*c; 
        double vy=direction.y*c;

        dr=(x*vx + y*vy)/r;
        dphi=(x*vy-y*vx)/(r*r);

        
        double f=1.0-rs/r;  //(Schwarzschild Factor)-represents the curvature of space
        double dt_dlambda=sqrt((dr*dr)/(f*f)+(r*r*dphi*dphi)/f);//it represents gravitational time dilation.It tells us how coordinate time t passes for every step the photon take..time slows down near the event horizon
        E=f*dt_dlambda;      //calculated  E using the null geodesic condition(ds2=0)This constant allows us to calculate how time dilates at future steps without re-solving the entire metric every frame

        // 5. OpenGL Buffers
        positions.push_back(startPos);
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, MAX_TRAIL_LENGTH * sizeof(TrailPoint), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TrailPoint), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(TrailPoint), (void*)(sizeof(vec2)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }
    // This function calculates how our variables change at any instant.
    //It tells the computer how gravity behaves. Specifically, it implements the Geodesic Equations derived from the Schwarzschild Metric
    //Inputs:
    //   state[0] -> r (Radius)
    //   state[1] -> phi (Angle)
    //   state[2] -> dr (Radial Velocity)
    //   state[3] -> dphi (Angular Velocity)
    void evaluate(double state[4], double output[4], double rs) {
        double _r=state[0];    //current radius
        double _dr=state[2];   //current radial velocity
        double _dphi=state[3]; //current angular velocity

        //Schwarzschild Factor
        double f=1.0-rs/_r;

        // 2. Calculate Time Dilation (dt/dlambda)
        // We use the conserved Energy 'E' we calculated in the constructor
        double dt_dl=E/f;

        // 3. Output the Derivatives (The "Changes")
        
        // Change in position is just velocity
        output[0] = _dr;   
        output[1] = _dphi; 

        // Change in Radial Velocity (Radial Acceleration)
        // This is the big scary Geodesic Equation!
        // It describes how gravity pulls the ray inward.
        output[2] = -(rs / (2.0*_r*_r)) * f * (dt_dl*dt_dl) 
                    + (rs / (2.0*_r*_r*f)) * (_dr*_dr) 
                    + (_r - rs) * (_dphi*_dphi);

        // Change in Angular Velocity (Angular Acceleration)
        // Conservation of angular momentum logic
        output[3] = -2.0 * _dr * _dphi / _r;
    }
    // STEP FUNCTION
    // --- STAGE 3: THE INTEGRATOR (RK4) ---
    void step(float dt, float rs) {
        // 1. If dead, do nothing
        if (absorbed) return;

        // 2. Prepare the RK4 Solver
        // We pack our variables into an array "y0" so we can pass them easily
        double y0[4] = {r, phi, dr, dphi};
        double k1[4], k2[4], k3[4], k4[4];
        double temp[4]; // Helper to hold intermediate states

        // --- STEP 1: Look at the start (k1) ---
        evaluate(y0, k1, rs);

        // --- STEP 2: Look halfway ahead using k1 (k2) ---
        for(int i=0; i<4; i++) temp[i] = y0[i] + k1[i] * dt * 0.5;
        evaluate(temp, k2, rs);

        // --- STEP 3: Look halfway ahead using k2 (k3) ---
        for(int i=0; i<4; i++) temp[i] = y0[i] + k2[i] * dt * 0.5;
        evaluate(temp, k3, rs);

        // --- STEP 4: Look fully ahead using k3 (k4) ---
        for(int i=0; i<4; i++) temp[i] = y0[i] + k3[i] * dt;
        evaluate(temp, k4, rs);

        // 3. Update Variables (Weighted Average)
        // This is the "Magic Formula" of RK4
        r    += (dt/6.0) * (k1[0] + 2*k2[0] + 2*k3[0] + k4[0]);
        phi  += (dt/6.0) * (k1[1] + 2*k2[1] + 2*k3[1] + k4[1]);
        dr   += (dt/6.0) * (k1[2] + 2*k2[2] + 2*k3[2] + k4[2]);
        dphi += (dt/6.0) * (k1[3] + 2*k2[3] + 2*k3[3] + k4[3]);

        // 4. Boundary Checks
        if (r < rs) {
            absorbed = true;
        }
        // Optimization: If it flies too far away, stop simulating it
        if (r > 2.0e11) {
            // Optional: You could mark it as 'escaped' to stop processing
        }

        // 5. Convert Physics (Polar) back to Visuals (Cartesian)
        // We need 'pos' so OpenGL knows where to draw the line
        pos.x = r * cos(phi);
        pos.y = r * sin(phi);

        // 6. Update Trail
        positions.push_back(pos);
        if (positions.size() > MAX_TRAIL_LENGTH) {
            positions.erase(positions.begin());
        }
        updateMesh();
    }

    void updateMesh() {
        if(positions.size() < 2) return; // Don't draw single points

        vector<TrailPoint> gpuData;
        gpuData.reserve(positions.size());
        size_t N = positions.size();
        
        for(size_t i = 0; i < N; i++) {
            TrailPoint pt;
            pt.pos = positions[i];
            
            // Safe fade calculation
            if (N > 1) {
                pt.alpha = (float)i / (float)(N - 1);
            } else {
                pt.alpha = 1.0f;
            }
            
            gpuData.push_back(pt);
        }
        
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, gpuData.size() * sizeof(TrailPoint), gpuData.data());
        glBindVertexArray(0);
    }

    void draw(GLuint shader) {
        if (positions.size() < 2) return;
        glUseProgram(shader);
        glUniform3f(glGetUniformLocation(shader, "uColor"), 1.0f, 1.0f, 1.0f);
        glLineWidth(1.0f);
        glBindVertexArray(vao);
        glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)positions.size());
        glBindVertexArray(0);
    }
};

// --- ENGINE ---
struct Engine {
    GLFWwindow* window;
    GLuint shader;
    Blackhole* blackhole;
    vector<Ray*> rays;

    bool init() {
        if (!glfwInit()) return false;
        window = glfwCreateWindow(1000, 750, "Black Hole Simulation", nullptr, nullptr);
        glfwMakeContextCurrent(window);
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return false;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        shader = createProgram();
        blackhole = new Blackhole(vec2(0, 0), 8.54e36);

        // Spawn Rays
        int rayCount = 20;
        float startX = -WORLD_WIDTH * 0.95f;
        for (int i = 0; i <= rayCount; i++) {
            float t = (float)i / (float)rayCount;
            float y = mix(-WORLD_HEIGHT * 0.9f, WORLD_HEIGHT * 0.9f, t);
            rays.push_back(new Ray(vec2(startX, y), vec2(1.0f, 0.0f), blackhole->r_s));
        }

        mat4 proj = ortho(-WORLD_WIDTH, WORLD_WIDTH, -WORLD_HEIGHT, WORLD_HEIGHT, -1.0f, 1.0f);
        glUseProgram(shader);
        glUniformMatrix4fv(glGetUniformLocation(shader, "uProjection"), 1, GL_FALSE, value_ptr(proj));
        return true;
    }

    void update(float dt) {
        float timeScale = 200.0f;
        for (Ray* r : rays) {
            r->step(dt * timeScale, blackhole->r_s);
        }
    }

    void render() {
        glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader);
        blackhole->draw(shader);
        for (Ray* r : rays) r->draw(shader);
    }
};

int main() {
    Engine engine;
    if (engine.init()) {
        float lastTime = (float)glfwGetTime();
        while (!glfwWindowShouldClose(engine.window)) {
            float currentTime = (float)glfwGetTime();
            float dt = currentTime - lastTime;
            lastTime = currentTime;
            engine.update(dt);
            engine.render();
            glfwSwapBuffers(engine.window);
            glfwPollEvents();
        }
    }
    return 0;
}