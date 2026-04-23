#version 430 core
layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec2 vertexUV;
layout (location = 2) in vec3 vertexNormal;
layout (location = 3) in vec3 vertexTangent;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;
uniform vec3 viewPos;

out vec3 fragPos;
out vec2 fragUV;
out vec3 fragNormal;
out mat3 TBN;
out vec3 tangentViewDir;

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
    vec3 B = cross(T, N);
    TBN = mat3(T, B, N);

    // View direction in tangent space for parallax mapping
    mat3 TBN_transpose = transpose(TBN);  // world to tangent
    tangentViewDir = normalize(TBN_transpose * (viewPos - fragPos));
}