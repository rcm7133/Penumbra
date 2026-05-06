#version 430 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform sampler2D heightMap;
uniform float rippleStrength;

void main() {
    TexCoords = aTexCoords;
    vec3 pos = aPos;

    float height = texture(heightMap, aTexCoords).r;
    height = clamp(height, -1.0, 1.0);
    pos.y += height * rippleStrength;

    vec4 worldPos = model * vec4(pos, 1.0);
    FragPos = worldPos.xyz;
    gl_Position = projection * view * worldPos;
}