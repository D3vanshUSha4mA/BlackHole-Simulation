#version 330 core
out vec4 FragColor;

uniform vec2 uResolution;
uniform vec3 uCamPos;

uniform float uMass;        // Controlled by UI slider
uniform float uExposure;    // Controlled by UI slider
uniform float uDiskSpread;  // Controlled by UI slider

const float base_dt = 0.05; 
const int MAX_STEPS = 1500;  

// --- PROCEDURAL 3D NOISE (FBM) ---
// Fast hash function for randomness
float hash(float n) { return fract(sin(n) * 43758.5453123); }

// Smooth 3D Value Noise
float noise(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f); // Smoothstep interpolation
    float n = p.x + p.y * 57.0 + 113.0 * p.z;
    return mix(mix(mix(hash(n +  0.0), hash(n +  1.0), f.x),
                   mix(hash(n + 57.0), hash(n + 58.0), f.x), f.y),
               mix(mix(hash(n + 113.0), hash(n + 114.0), f.x),
                   mix(hash(n + 170.0), hash(n + 171.0), f.x), f.y), f.z);
}

// Fractal Brownian Motion (3 octaves for performance)
float fbm(vec3 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 3; i++) {
        v += a * noise(p);
        p = p * 2.0;
        a *= 0.5;
    }
    return v;
}
// ----------------------------------

// Schwarzschild Metric Acceleration
vec3 getAcceleration(vec3 pos, vec3 vel) {
    float r2 = dot(pos, pos); 
    float r = sqrt(r2);       
    float r5 = r2 * r2 * r;   
    vec3 L = cross(pos, vel);
    float L2 = dot(L, L);     
    return -1.5 * uMass * L2 / r5 * pos; // Uses interactive Mass
}

float getGridHeight(vec3 p) {
    return -(1.5 * uMass) / max(length(p.xz), 0.1); // Uses interactive Mass
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
    vec3 rd = normalize(uv.x * right + uv.y * up + 1.5 * forward); 

    vec3 pos = ro;
    vec3 vel = rd; 
    bool absorbed = false;
    vec3 finalColor = vec3(0.0); 

    float diskInner = 2.6 * uMass; 
    float diskOuter = uDiskSpread * uMass; // Uses interactive Disk Spread
    float diskThickness = 0.35 * uMass; 
    vec3 gridColor = vec3(0.8, 0.3, 0.2); 
    
    // Beer-Lambert Optical Depth
    float tau = 0.0; 

    // The GPU parallelized RK4 Loop
    for (int i = 0; i < MAX_STEPS; i++) {
        vec3 old_pos = pos; 
        
        float r_current = length(pos);
        float dynamic_dt = clamp(base_dt * (r_current / (1.5 * uMass)), 0.005, 0.2);
        
        vec3 k1_v = getAcceleration(pos, vel);
        vec3 k1_p = vel;
        vec3 k2_v = getAcceleration(pos + k1_p * dynamic_dt * 0.5, vel + k1_v * dynamic_dt * 0.5);
        vec3 k2_p = vel + k1_v * dynamic_dt * 0.5;
        vec3 k3_v = getAcceleration(pos + k2_p * dynamic_dt * 0.5, vel + k2_v * dynamic_dt * 0.5);
        vec3 k3_p = vel + k2_v * dynamic_dt * 0.5;
        vec3 k4_v = getAcceleration(pos + k3_p * dynamic_dt, vel + k3_v * dynamic_dt);
        vec3 k4_p = vel + k3_v * dynamic_dt;
        
        pos += (dynamic_dt / 6.0) * (k1_p + 2.0*k2_p + 2.0*k3_p + k4_p);
        vel += (dynamic_dt / 6.0) * (k1_v + 2.0*k2_v + 2.0*k3_v + k4_v);

        // --- VOLUMETRIC ACCRETION DISK (BEER-LAMBERT) ---
        // 1. Check if we are inside the 3D bounding box of the gas cloud
        if (abs(pos.y) < diskThickness) {
            float r_disk = length(pos.xz);
            if (r_disk > diskInner && r_disk < diskOuter) {
                
                // 2. Twist coordinates to make the noise look like orbital bands
                float angle = atan(pos.z, pos.x) - r_disk * 2.0; 
                vec3 noisePos = vec3(cos(angle) * r_disk, pos.y * 3.0, sin(angle) * r_disk);
                
                // 3. Sample FBM to get raw plasma density
                float density = fbm(noisePos * 8.0);
                
                // 4. Shape the cloud (Fade out vertically and at the edges)
                density *= smoothstep(diskThickness, 0.0, abs(pos.y)); 
                density *= smoothstep(diskInner, diskInner + 0.5, r_disk); 
                density *= 1.0 - smoothstep(diskOuter - 1.5, diskOuter, r_disk); 
                
                if (density > 0.05) {
                    // Relativistic Doppler Math 
                    float v_mag = sqrt(uMass / (2.0 * r_disk));
                    vec3 v_dir = normalize(vec3(-pos.z, 0.0, pos.x));
                    vec3 v_vec = v_dir * v_mag;
                    vec3 photon_dir = -normalize(vel);
                    float v_dot = dot(v_vec, photon_dir);
                    float doppler = sqrt(1.0 - (v_mag * v_mag)) / (1.0 - v_dot);
                    float beaming = pow(doppler, 3.0);
                    
                    vec3 redshiftColor = vec3(1.0, 0.1, 0.0);
                    vec3 blueshiftColor = vec3(0.0, 0.8, 1.0);
                    vec3 gasColor = mix(redshiftColor, blueshiftColor, smoothstep(0.5, 1.5, doppler));
                    gasColor = mix(gasColor, vec3(1.0, 1.0, 0.8), density * 0.5);

                    // 5. Beer-Lambert Accumulation
                    float absorption_coefficient = 4.0; 
                    float step_tau = density * absorption_coefficient * dynamic_dt;
                    tau += step_tau; 
                    
                    float emission_strength = 6.0;
                    vec3 emission = gasColor * density * emission_strength * beaming;
                    
                    // Add light to final color, but block it based on gas we've already passed through (exp(-tau))
                    finalColor += emission * exp(-tau) * dynamic_dt;
                    
                    // Optimization: If the gas is completely opaque, stop raymarching the disk
                    if (tau > 5.0) break; 
                }
            }
        }
        // ------------------------------------------------

        // Curved Grid
        float h_old = old_pos.y - getGridHeight(old_pos);
        float h_new = pos.y - getGridHeight(pos);
        if (h_old * h_new < 0.0) {
            float t_grid = h_old / (h_old - h_new);
            vec3 gridIntersect = mix(old_pos, pos, t_grid);
            float r_grid = length(gridIntersect.xz);
            if (r_grid > uMass * 1.1 && tau < 4.0) { // Grid is obscured by thick gas
                vec2 gridPattern = fract(gridIntersect.xz);
                float grid = max(1.0 - smoothstep(0.03, 0.05, abs(gridPattern.x - 0.5) * 2.0),
                                 1.0 - smoothstep(0.03, 0.05, abs(gridPattern.y - 0.5) * 2.0));
                float gridFade = 1.0 - smoothstep(10.0, 30.0, r_grid);
                // Grid is blocked by the gas in front of it
                finalColor += gridColor * grid * gridFade * 0.5 * exp(-tau); 
            }
        }

        if (length(pos) < uMass) { absorbed = true; break; }
        if (length(pos) > 35.0) break; 
    }

    if (absorbed && tau < 0.1) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0); 
    } else {
        // Uses the interactive Exposure slider directly
        vec3 mapped = vec3(1.0) - exp(-finalColor * uExposure);
        mapped = pow(mapped, vec3(1.0 / 2.2));
        FragColor = vec4(mapped + vec3(0.01, 0.01, 0.03), 1.0); 
    }
}