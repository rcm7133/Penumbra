#version 430 core

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(r16f, binding = 0) uniform image2D heightMap;

uniform ivec2 ripplePos;
uniform float strength;
uniform int radius;

void main() {
    // Write a circular disturbance around ripplePos
    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {
            float dist = length(vec2(x, y));
            if (dist > float(radius)) continue;

            float falloff = 1.0 - (dist / float(radius));
            ivec2 texel = ripplePos + ivec2(x, y);

            float current = imageLoad(heightMap, texel).r;
            imageStore(heightMap, texel, vec4(current + strength * falloff));
        }
    }
}