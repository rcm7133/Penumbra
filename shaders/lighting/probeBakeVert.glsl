#version 430 core
layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec2 vertexUV;
layout (location = 2) in vec3 vertexNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

out vec3 fragPos;
out vec2 fragUV;
out vec3 fragNormal;

void main()
{
    vec4 worldPos = model * vec4(vertexPos, 1.0);
    fragPos = worldPos.xyz;
    fragUV = vertexUV;
    fragNormal = normalize(normalMatrix * vertexNormal);
    gl_Position = projection * view * worldPos;
}