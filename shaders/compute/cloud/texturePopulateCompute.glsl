#version 430 core

layout (local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(std430, binding = 0) buffer pointDataR
{
    vec4 pointRSSBO[];
};
layout(std430, binding = 1) buffer pointDataG
{
    vec4 pointGSSBO[];
};
layout(std430, binding = 2) buffer pointDataB
{
    vec4 pointBSSBO[];
};
layout(std430, binding = 3) buffer pointDataA
{
    vec4 pointASSBO[];
};

layout(rgba16f, binding = 4) uniform image3D noiseTexture;

uniform int voxelResolutionR;
uniform int voxelResolutionG;
uniform int voxelResolutionB;
uniform int voxelResolutionA;
uniform int inverted;

int XYZToIndex(ivec3 coord, int voxelRes)
{
    coord = ((coord % voxelRes) + voxelRes) % voxelRes;
    return coord.x + coord.y * voxelRes + coord.z * voxelRes * voxelRes;
}

float worley(vec3 pos, int voxelRes, int channel)
{
    ivec3 voxelCoord = ivec3(pos * float(voxelRes));
    float minDist = 1e9;

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                ivec3 neighborVoxel = voxelCoord + ivec3(x, y, z);
                vec3 offset = vec3(
                neighborVoxel.x < 0 ? -1.0 : (neighborVoxel.x >= voxelRes ? 1.0 : 0.0),
                neighborVoxel.y < 0 ? -1.0 : (neighborVoxel.y >= voxelRes ? 1.0 : 0.0),
                neighborVoxel.z < 0 ? -1.0 : (neighborVoxel.z >= voxelRes ? 1.0 : 0.0)
                );

                vec3 point;
                int idx = XYZToIndex(neighborVoxel, voxelRes);
                if (channel == 0) point = pointRSSBO[idx].xyz;
                else if (channel == 1) point = pointGSSBO[idx].xyz;
                else if (channel == 2) point = pointBSSBO[idx].xyz;
                else point = pointASSBO[idx].xyz;
                point += offset;

                minDist = min(minDist, distance(pos, point));
            }
        }
    }

    float maxDist = sqrt(3.0) / float(voxelRes);
    return clamp(minDist / maxDist, 0.0, 1.0);
}

void main()
{
    ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
    ivec3 texSize = imageSize(noiseTexture);
    if (coord.x >= texSize.x || coord.y >= texSize.y || coord.z >= texSize.z) return;

    vec3 pos = vec3(coord) / vec3(texSize);

    float r = worley(pos, voxelResolutionR, 0);
    float g = worley(pos, voxelResolutionG, 1);
    float b = worley(pos, voxelResolutionB, 2);
    float a = worley(pos, voxelResolutionA, 3);

    vec4 finalOutput = vec4(r, g, b, a);
    if (inverted == 1) finalOutput = vec4(1.0) - finalOutput;

    imageStore(noiseTexture, coord, finalOutput);
}
