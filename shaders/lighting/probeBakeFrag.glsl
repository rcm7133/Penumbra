#version 430 core
#include "../common/lights.glsl"

in vec3 fragPos;
in vec2 fragUV;
in vec3 fragNormal;

out vec4 fragColor;

uniform sampler2D diffuseTex;
uniform float roughness;
uniform float metallic;
uniform vec3 cameraPos;

void main()
{
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(cameraPos - fragPos);
    vec3 albedo = texture(diffuseTex, fragUV).rgb;

    // Simple diffuse + ambient for probe baking
    vec3 result = ambientMultiplier * albedo;

    for (int i = 0; i < lightCount; i++)
    {
        vec3 L;
        float attenuation = 1.0;
        float spotEffect = 1.0;

        if (lights[i].type == LIGHT_DIRECTIONAL) {
            L = normalize(-lights[i].direction);
        } else if (lights[i].type == LIGHT_POINT) {
            L = normalize(lights[i].position - fragPos);
            float dist = length(lights[i].position - fragPos);
            attenuation = 1.0 / (dist * dist);
            attenuation *= 1.0 - smoothstep(lights[i].radius * 0.75, lights[i].radius, dist);
        } else {
            L = normalize(lights[i].position - fragPos);
            float dist = length(lights[i].position - fragPos);
            attenuation = 1.0 / (dist * dist);
            float theta = dot(L, normalize(-lights[i].direction));
            float epsilon = lights[i].innerCutoff - lights[i].outerCutoff;
            spotEffect = clamp((theta - lights[i].outerCutoff) / epsilon, 0.0, 1.0);
        }

        float NdotL = max(dot(N, L), 0.0);
        vec3 diffuse = NdotL * lights[i].color * lights[i].intensity * albedo;

        // Simple shadow sampling for shadow-casting lights
        float shadow = 0.0;
        if (i < shadowLightCount) {
            if (lights[i].type == LIGHT_POINT)
            shadow = SamplePointShadow(shadowCubeMap[i], fragPos, lights[i].position, lightFarPlane[i]);
            else
            shadow = SampleShadowMap(shadowMap[i], lightSpaceMatrix[i], fragPos, N, L);
        }

        result += (1.0 - shadow) * diffuse * attenuation * spotEffect;
    }

    fragColor = vec4(result, 1.0);
}