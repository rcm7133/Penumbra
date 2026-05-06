#version 430 core

layout(local_size_x = 8, local_size_y = 8) in;

layout(rgba16f, binding = 0) uniform image3D sourceTexture;
layout(rgba8,   binding = 1) uniform image2D outputTexture;

uniform float slice;     // 0.0 - 1.0
uniform int   channel;   // 0=R, 1=G, 2=B, 3=A, 4=RGB

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec3 size  = imageSize(sourceTexture);
    if (coord.x >= size.x || coord.y >= size.y) return;

    int z = int(slice * float(size.z - 1));
    vec4 texel = imageLoad(sourceTexture, ivec3(coord.x, coord.y, z));

    vec4 finalOutput;
    switch (channel) {
        case 0:  finalOutput = vec4(texel.rrr, 1.0); break;
        case 1:  finalOutput = vec4(texel.ggg, 1.0); break;
        case 2:  finalOutput = vec4(texel.bbb, 1.0); break;
        case 3:  finalOutput = vec4(texel.aaa, 1.0); break;
        default:
            float grey = dot(texel.rgb, vec3(0.299, 0.587, 0.114));
            finalOutput = vec4(grey, grey, grey, 1.0);
            break;
    }

    imageStore(outputTexture, coord, finalOutput);
}