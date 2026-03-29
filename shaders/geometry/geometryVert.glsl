#version 330 core
layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec2 vertexUV;
layout (location = 2) in vec3 vertexNormal;
layout (location = 3) in vec3 vertexTangent;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

out vec3 fragPos;
out vec2 fragUV;
out vec3 fragNormal;
out mat3 TBN;

void main()
{
    vec4 worldPos   = model * vec4(vertexPos, 1.0);
    gl_Position     = projection * view * worldPos;
    fragPos         = worldPos.xyz;
    fragUV          = vertexUV;
    fragNormal      = normalMatrix * vertexNormal;

    // Build TBN matrix in world space
    vec3 T = normalize(normalMatrix * vertexTangent);
    vec3 N = normalize(fragNormal);
    T = normalize(T - dot(T, N) * N);   // re-orthogonalize
    vec3 B = cross(N, T);
    TBN = mat3(T, B, N);
}