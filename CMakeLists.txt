cmake_minimum_required(VERSION 3.5)

project(Penumbra VERSION 1.0)

if(UNIX AND NOT APPLE)
    cmake_policy(SET CMP0072 NEW)
    set(OpenGL_GL_PREFERENCE GLVND)
endif()

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

add_executable(Penumbra
        src/main.cpp
        src/mesh.cpp
        src/glad.c
        src/stb_image.cpp
        src/renderer.cpp
        src/settings.cpp
        dependencies/imgui/imgui.cpp
        dependencies/imgui/imgui_draw.cpp
        dependencies/imgui/imgui_tables.cpp
        dependencies/imgui/imgui_widgets.cpp
        dependencies/imgui/imgui_impl_glfw.cpp
        dependencies/imgui/imgui_impl_opengl3.cpp
        src/profiler.h
)

target_include_directories(Penumbra PRIVATE
        src
        dependencies
        dependencies/imgui
)

target_link_libraries(Penumbra
        glfw
        OpenGL::GL
        glm::glm
)

target_compile_definitions(Penumbra PRIVATE NOMINMAX)
target_compile_features(Penumbra PRIVATE cxx_std_23)

if(UNIX)
    target_link_libraries(Penumbra dl)
endif()

if(WIN32)
    target_link_libraries(Penumbra gdi32 user32)
endif()