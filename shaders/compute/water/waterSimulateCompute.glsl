#version 430 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(r16f, binding = 0) uniform readonly image2D currentMap;
layout(r16f, binding = 1) uniform readonly image2D previousMap;
layout(r16f, binding = 2) uniform writeonly image2D outputMap;


uniform float waveSpeed;
uniform float dampening;

void main() {
    ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(currentMap);
    if (texel.x >= size.x || texel.y >= size.y) return;

    float curr  = imageLoad(currentMap,  texel).r;
    float prev  = imageLoad(previousMap, texel).r;
    float up    = imageLoad(currentMap,  texel + ivec2(0,  1)).r;
    float down  = imageLoad(currentMap,  texel + ivec2(0, -1)).r;
    float left  = imageLoad(currentMap,  texel + ivec2(-1, 0)).r;
    float right = imageLoad(currentMap,  texel + ivec2(1,  0)).r;

    float laplacian = left + right + up + down - 4.0 * curr;
    float next = 2.0 * curr - prev + waveSpeed * waveSpeed * laplacian;
    next *= dampening;
    next = clamp(next, -1.0, 1.0);

    imageStore(outputMap, texel, vec4(next));
}