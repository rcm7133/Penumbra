#version 430 core

#include "../common/lights.glsl"
#include "../common/reflectionProbes.glsl"

out vec4 FragColor;

in vec3 FragPos;
in vec2 TexCoords;

uniform vec3 cameraPos;
uniform vec3 shallowColor;
uniform vec3 deepColor;
uniform float fresnelPower;
uniform float specularStrength;

uniform sampler2D normalMap;

uniform sampler2D mossTex;      // density map
uniform sampler2D mossAlbedo;
uniform float mossOpacity;

void main()
{
    vec3 normal = texture(normalMap, TexCoords).rgb * 2.0 - 1.0;
    normal = normalize(normal);


    vec3 viewDir = normalize(cameraPos - FragPos);

    float fresnel = pow(1.0 - max(dot(viewDir, normal), 0.0), fresnelPower);

    vec3 waterColor = mix(shallowColor, deepColor, fresnel);

    // Lighting from scene lights
    vec3 specular = vec3(0.0);
    for (int i = 0; i < lightCount; i++) {
        Light light = lights[i];

        vec3 toLight;
        float attenuation = 1.0;

        if (light.type == LIGHT_DIRECTIONAL) {
            toLight = normalize(-light.direction);
        } else {
            vec3 delta = light.position - FragPos;
            float dist = length(delta);
            toLight = normalize(delta);
            attenuation = clamp(1.0 - dist / light.radius, 0.0, 1.0);
            attenuation *= attenuation;
        }

        // Specular
        vec3 halfVec = normalize(viewDir + toLight);
        float spec = pow(max(dot(normal, halfVec), 0.0), 128.0);
        specular += specularStrength * spec * light.color * light.intensity * attenuation;

        // Shadows
        for (int s = 0; s < shadowLightCount; s++) {
            if (light.type == LIGHT_POINT) {
                float shadow = SamplePointShadow(shadowCubeMap[s], FragPos,
                light.position, lightFarPlane[s]);
                specular *= (1.0 - shadow * 0.8);
            } else {
                float shadow = SampleShadowMap(shadowMap[s], lightSpaceMatrix[s],
                FragPos, normal, toLight);
                specular *= (1.0 - shadow * 0.8);
            }
        }
    }

    vec3 reflectDir = reflect(-viewDir, normal);
    vec3 reflection = SampleReflectionProbes(reflectDir, FragPos, 0.0);


    // Combine
    vec3 color = mix(waterColor, reflection, fresnel * 0.8);
    color += specular;

    float mossDensity = texture(mossTex, TexCoords).r;
    vec3 mossColor    = texture(mossAlbedo, TexCoords).rgb;

    color = mix(color, mossColor, mossDensity * mossOpacity);
    float alpha = mix(0.5, 0.98, fresnel);
    alpha = max(alpha, mossDensity * mossOpacity);

    FragColor = vec4(color, alpha);
}