#version 430 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    TexCoords = aTexCoords;
    vec3 pos = aPos;

    vec4 worldPos = model * vec4(pos, 1.0);
    FragPos = worldPos.xyz;
    gl_Position = projection * view * worldPos;
}