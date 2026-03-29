#version 330 core
out float fragColor;

in vec2 fragUV;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D noiseTexture;

uniform vec3 samples[64];
uniform mat4 projection;
uniform mat4 view;

uniform vec2 screenSize;
uniform float radius;
uniform float bias;
uniform int kernelSize;

void main()
{
    vec2 noiseScale = screenSize / 4.0;

    vec3 fragPos = (view * vec4(texture(gPosition, fragUV).xyz, 1.0)).xyz;
    vec3 normal  = normalize((view * vec4(texture(gNormal, fragUV).xyz, 0.0)).xyz);
    vec3 rand    = normalize(texture(noiseTexture, fragUV * noiseScale).xyz);

    // Gram Schmidt to build TBN from random vector
    vec3 tangent   = normalize(rand - normal * dot(rand, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < kernelSize; i++)
    {
        // Sample position in view space
        vec3 samplePos = fragPos + TBN * samples[i] * radius;

        // Project to screen space
        vec4 offset = projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz  = offset.xyz * 0.5 + 0.5;

        // Get depth of scene at that screen position in view space
        float sampleDepth = (view * vec4(texture(gPosition, offset.xy).xyz, 1.0)).z;

        // Range check to avoid halo artifacts
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }

    fragColor = 1.0 - (occlusion / float(kernelSize));
}