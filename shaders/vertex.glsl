#version 330 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec2 vertexUV;
layout (location = 2) in vec3 vertexNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 fragmentUV;
out vec3 fragmentNormal;
out vec3 fragmentPos;

uniform mat3 normalMatrix;

void main()
{
    gl_Position = projection * view * model * vec4(vertexPos, 1.0);
    fragmentUV = vertexUV;
    fragmentNormal  = normalMatrix  * vertexNormal;
    fragmentPos     = vec3(model * vec4(vertexPos, 1.0));
}