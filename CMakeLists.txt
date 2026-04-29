cmake_minimum_required(VERSION 3.5)

project(Penumbra VERSION 1.0)

if(UNIX AND NOT APPLE)
    cmake_policy(SET CMP0072 NEW)
    set(OpenGL_GL_PREFERENCE GLVND)
endif()

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_package(OpenMP REQUIRED)

add_executable(Penumbra
        src/main.cpp
        src/rendering/mesh/mesh.cpp
        dependencies/glad/glad.c
        src/rendering/stb_image.cpp
        src/rendering/renderer.cpp
        src/settings.cpp
        src/utils/shaderutils.cpp
        src/rendering/debug/colliderDebugRenderer.cpp
        src/rendering/mesh/model.cpp
        src/rendering/effects/water/interactiveWater.cpp
        src/rendering/pathTracing/pathTracer.cpp
        src/rendering/effects/clouds/worleyNoise.cpp
        src/utils/sceneLoader.cpp
        src/physics/camera/characterControllerComponent.cpp
        dependencies/imgui/imgui.cpp
        dependencies/imgui/imgui_draw.cpp
        dependencies/imgui/imgui_tables.cpp
        dependencies/imgui/imgui_widgets.cpp
        dependencies/imgui/imgui_impl_glfw.cpp
        dependencies/imgui/imgui_impl_opengl3.cpp
        src/utils/profiler.h
)

target_include_directories(Penumbra PRIVATE
        src
        stc/utils
        src/particles
        dependencies
        dependencies/imgui
)

target_link_libraries(Penumbra PRIVATE
        glfw
        OpenGL::GL
        glm::glm
        Jolt::Jolt
)

if(OpenMP_CXX_FOUND)
    target_link_libraries(Penumbra PRIVATE OpenMP::OpenMP_CXX)
    target_compile_options(Penumbra PRIVATE ${OpenMP_CXX_FLAGS})
endif()

if(MSVC)
    target_compile_options(Penumbra PRIVATE /openmp)
endif()

target_compile_definitions(Penumbra PRIVATE NOMINMAX)
target_compile_features(Penumbra PRIVATE cxx_std_23)

if(UNIX)
    target_link_libraries(Penumbra PRIVATE dl)
endif()

if(WIN32)
    target_link_libraries(Penumbra PRIVATE gdi32 user32)
endif()