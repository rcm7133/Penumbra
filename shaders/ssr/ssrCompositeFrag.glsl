#version 430 core
#include "../common/reflectionProbes.glsl"

in vec2 fragUV;
out vec4 FragColor;

// Base lit scene (already has direct lighting + SH GI)
uniform sampler2D litScene;

// SSR result (rgb = reflection, a = hit confidence)
uniform sampler2D ssrTexture;
uniform sampler2D gAlbedo;
// GBuffer data
uniform sampler2D gNormal;   // rgb = normal, a = roughness
uniform sampler2D gPosition;

uniform int ssrEnabled;
uniform int probeEnabled;

uniform vec3 cameraPos;

void main()
{
    vec3 baseColor = texture(litScene, fragUV).rgb;

    vec4 normalRough = texture(gNormal, fragUV);
    vec3 normal = normalize(normalRough.xyz);
    float roughness = normalRough.a;
    float metallic = texture(gAlbedo, fragUV).a;

    if (ssrEnabled == 0 && probeEnabled == 0) {
        FragColor = vec4(baseColor, 1.0);
        return;
    }

    vec3 worldPos = texture(gPosition, fragUV).xyz;
    vec3 viewDir = normalize(cameraPos - worldPos);
    vec3 reflDir = reflect(-viewDir, normal);

    // Fresnel
    float f0 = mix(0.04, 1.0, metallic);
    float fresnel = f0 + (1.0 - f0) * pow(1.0 - max(dot(normal, viewDir), 0.0), 5.0);

    float reflectivity = fresnel * (1.0 - roughness);

    // SSR hits take priority
    vec4 ssrSample = texture(ssrTexture, fragUV);
    vec3 ssrColor = ssrSample.rgb;
    float ssrConfidence = ssrSample.a;

    // Probe fallback where SSR misses
    vec3 probeColor = SampleReflectionProbes(reflDir, worldPos, roughness);

    // SSR wins on hits, probe fills gaps
    vec3 reflection = vec3(0.0);

    if (ssrEnabled == 1 && probeEnabled == 1) {
        vec4 ssrSample = texture(ssrTexture, fragUV);
        vec3 probeColor = SampleReflectionProbes(reflDir, worldPos, roughness);
        reflection = mix(probeColor, ssrSample.rgb, ssrSample.a);
    } else if (ssrEnabled == 1) {
        vec4 ssrSample = texture(ssrTexture, fragUV);
        reflection = ssrSample.rgb * ssrSample.a;
    } else {
        reflection = SampleReflectionProbes(reflDir, worldPos, roughness);
    }

    FragColor = vec4(baseColor + reflection * reflectivity, 1.0);
}