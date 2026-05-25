cmake_minimum_required(VERSION 3.10)
project(RelativisticBlackHole VERSION 1.0)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Point to your local header files and our new core folder
include_directories(include)
include_directories(core)

# --- TARGETS ---

# Target 1: 2D Prototype
add_executable(prototype_2d src/prototype_2d.cpp src/glad.c)
# Link directly to the DLL in your root folder + Windows OpenGL
target_link_libraries(prototype_2d ${CMAKE_SOURCE_DIR}/glfw3.dll opengl32) 

# Target 2: CPU-Bound 3D Prototype
add_executable(prototype_3d_cpu src/prototype_3d_cpu.cpp src/glad.c)
target_link_libraries(prototype_3d_cpu ${CMAKE_SOURCE_DIR}/glfw3.dll opengl32)

# --- Target 3: GPU-Accelerated 3D Engine with ImGui ---
# 1. Define all the new UI source files we added
set(IMGUI_SOURCES 
    src/imgui/imgui.cpp 
    src/imgui/imgui_draw.cpp 
    src/imgui/imgui_tables.cpp 
    src/imgui/imgui_widgets.cpp 
    src/imgui/imgui_impl_glfw.cpp 
    src/imgui/imgui_impl_opengl3.cpp
)

# 2. Add them to the executable
add_executable(prototype_3d_gpu src/prototype_3d_gpu.cpp src/glad.c ${IMGUI_SOURCES})

# 3. Tell the compiler where to find the headers
target_include_directories(prototype_3d_gpu PRIVATE src/imgui)

# 4. Link the required libraries
target_link_libraries(prototype_3d_gpu ${CMAKE_SOURCE_DIR}/glfw3.dll opengl32)