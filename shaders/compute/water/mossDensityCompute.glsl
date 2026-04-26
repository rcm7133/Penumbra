#version 430 core

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(r16f, binding = 0) uniform readonly  image2D heightMap;
layout(r16f, binding = 1) uniform readonly  image2D mossInput;
layout(r16f, binding = 2) uniform writeonly image2D mossOutput;

uniform float flowRate;
uniform float viscosity;
uniform float deltaTime;

void main()
{
    ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(heightMap);
    if (texel.x >= size.x || texel.y >= size.y) return;

    float hL = imageLoad(heightMap, texel + ivec2(-1,  0)).r;
    float hR = imageLoad(heightMap, texel + ivec2( 1,  0)).r;
    float hU = imageLoad(heightMap, texel + ivec2( 0,  1)).r;
    float hD = imageLoad(heightMap, texel + ivec2( 0, -1)).r;

    float dhdx = hR - hL;
    float dhdz = hU - hD;

    vec2 velocity = -vec2(dhdx, dhdz);
    float speed = length(velocity);

    float density = imageLoad(mossInput, texel).r;

    float outflow = density * flowRate * clamp(speed / viscosity, 0.0, 1.0) * deltaTime;
    float remaining = density - outflow;

    float inflow = 0.0;

    // Left neighbour does its velocity point right
    float nDensL = imageLoad(mossInput, texel + ivec2(-1, 0)).r;
    float nhL    = imageLoad(heightMap, texel + ivec2(-2, 0)).r;
    float nhR    = imageLoad(heightMap, texel + ivec2( 0, 0)).r;
    float nVelX  = -(nhR - nhL); // left neighbour's X velocity
    inflow += max(nVelX, 0.0) * nDensL * flowRate * clamp(abs(nVelX) / viscosity, 0.0, 1.0) * deltaTime;

    // Right neighbour does its velocity point left
    float nDensR = imageLoad(mossInput, texel + ivec2( 1, 0)).r;
    nhL = imageLoad(heightMap, texel + ivec2( 0, 0)).r;
    nhR = imageLoad(heightMap, texel + ivec2( 2, 0)).r;
    float nVelXR = -(nhR - nhL);
    inflow += max(-nVelXR, 0.0) * nDensR * flowRate * clamp(abs(nVelXR) / viscosity, 0.0, 1.0) * deltaTime;

    // Up neighbour
    float nDensU = imageLoad(mossInput, texel + ivec2( 0,  1)).r;
    float nhU2   = imageLoad(heightMap, texel + ivec2( 0,  2)).r;
    float nhD2   = imageLoad(heightMap, texel + ivec2( 0,  0)).r;
    float nVelZ  = -(nhU2 - nhD2);
    inflow += max(-nVelZ, 0.0) * nDensU * flowRate * clamp(abs(nVelZ) / viscosity, 0.0, 1.0) * deltaTime;

    // Down neighbour
    float nDensDn = imageLoad(mossInput, texel + ivec2( 0, -1)).r;
    nhU2 = imageLoad(heightMap, texel + ivec2( 0,  0)).r;
    nhD2 = imageLoad(heightMap, texel + ivec2( 0, -2)).r;
    float nVelZD = -(nhU2 - nhD2);
    inflow += max(nVelZD, 0.0) * nDensDn * flowRate * clamp(abs(nVelZD) / viscosity, 0.0, 1.0) * deltaTime;

    float result = clamp(remaining + inflow, 0.0, 1.0);
    imageStore(mossOutput, texel, vec4(result));
}