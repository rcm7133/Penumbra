#version 430 core
#include "../../common/clouds.glsl"

layout (local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

uniform ivec3 voxelResolution;
uniform int lightingSteps;
uniform float lightingStepScale;
uniform float cloudAbsorption;

uniform vec3 lightColor;
uniform vec3 lightDir;
uniform float lightIntensity;

layout(rgba16f, binding = 0) uniform image3D voxelOutput;

void main()
{
    ivec3 coord = ivec3(gl_GlobalInvocationID);
    if (any(greaterThanEqual(coord, voxelResolution))) return;

    // Voxel Center in world space
    vec3 t = (vec3(coord) + 0.5) / vec3(voxelResolution);
    vec3 worldPos = cloudMin + t * (cloudMax - cloudMin);

    float transmittance = 1.0;
    float stepSize = length(cloudMax - cloudMin) / lightingStepScale;

    for (int i = 0; i < lightingSteps; i++)
    {
        vec3 samplePos = worldPos + lightDir * stepSize * float(i);
        if (any(lessThan(samplePos, cloudMin)) || any(greaterThan(samplePos, cloudMax))) {
            break;
        }

        float density = SampleDensity(samplePos);

        transmittance *= exp(-density * stepSize * cloudAbsorption);
        if (transmittance < 0.01) {
            break;
        }
    }

    vec3 lightContribution = lightColor * lightIntensity * transmittance;
    imageStore(voxelOutput, coord, vec4(lightContribution, 1.0));
}
