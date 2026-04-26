#version 430 core

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(r16f, binding = 0) uniform readonly image2D heightMap;
layout(rgba16f, binding = 1) uniform writeonly image2D normalMap;

uniform float heightScale; // match rippleStrength in vert shader
uniform float texelSize; // 1 / resolution

void main()
{
    ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size  = imageSize(heightMap);
    if (texel.x >= size.x || texel.y >= size.y) return;

    // Sample neighbors
    float hL = imageLoad(heightMap, texel + ivec2(-1,  0)).r; // left
    float hR = imageLoad(heightMap, texel + ivec2( 1,  0)).r; // right
    float hD = imageLoad(heightMap, texel + ivec2( 0, -1)).r; // down
    float hU = imageLoad(heightMap, texel + ivec2( 0,  1)).r; // up

    // Centeral difference
    vec3 normal = normalize(vec3(
        (hL - hR) * heightScale, // dH/dx
        2 * texelSize, // step size in Y
        (hD - hU) * heightScale // dH/dz
    ));

    // pack from -1 to 1 into 0 to 1
    imageStore(normalMap, texel, vec4(normal * 0.5 + 0.5, 1.0));
}