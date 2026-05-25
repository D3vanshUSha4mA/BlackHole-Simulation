#pragma once
#include <glm/glm.hpp>

namespace Physics {

    // The Schwarzschild radius constant (can be made dynamic later)
    constexpr float RS = 1.0f;

    // Calculates the acceleration of a photon in curved spacetime
    // Derived from the 3D Geodesic Equation
    inline glm::vec3 getAcceleration(const glm::vec3& pos, const glm::vec3& vel) {
        float r2 = glm::dot(pos, pos);
        float r = std::sqrt(r2);
        float r5 = r2 * r2 * r;
        
        // Angular momentum vector
        glm::vec3 L = glm::cross(pos, vel);
        float L2 = glm::dot(L, L);
        
        // Gravity pulls the light inward based on its angular momentum
        return -1.5f * RS * L2 / r5 * pos;
    }

    // 4th-Order Runge-Kutta (RK4) Integrator
    // Updates position and velocity with high precision
    inline void stepRK4(glm::vec3& pos, glm::vec3& vel, float dt) {
        glm::vec3 k1_v = getAcceleration(pos, vel);
        glm::vec3 k1_p = vel;

        glm::vec3 k2_v = getAcceleration(pos + k1_p * dt * 0.5f, vel + k1_v * dt * 0.5f);
        glm::vec3 k2_p = vel + k1_v * dt * 0.5f;

        glm::vec3 k3_v = getAcceleration(pos + k2_p * dt * 0.5f, vel + k2_v * dt * 0.5f);
        glm::vec3 k3_p = vel + k2_v * dt * 0.5f;

        glm::vec3 k4_v = getAcceleration(pos + k3_p * dt, vel + k3_v * dt);
        glm::vec3 k4_p = vel + k3_v * dt;

        pos += (dt / 6.0f) * (k1_p + 2.0f * k2_p + 2.0f * k3_p + k4_p);
        vel += (dt / 6.0f) * (k1_v + 2.0f * k2_v + 2.0f * k3_v + k4_v);
    }
}