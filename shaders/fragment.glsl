#version 330 core

in vec2 fragmentUV;
in vec3 fragmentNormal;
in vec3 fragmentPos;

out vec4 screenColor;

uniform vec3 cameraPos;
uniform float shininess;
uniform float ambientMultiplier;
uniform sampler2D diffuseTex;

#define MAX_LIGHTS 8

uniform vec3  lightPos[MAX_LIGHTS];
uniform vec3  lightColor[MAX_LIGHTS];
uniform float lightIntensity[MAX_LIGHTS];
uniform int   lightCount;

void main()
{
    vec3 texColor = texture(diffuseTex, fragmentUV).rgb;
    vec3 normal   = normalize(fragmentNormal);
    vec3 viewDir  = normalize(cameraPos - fragmentPos);

    vec3 ambient = ambientMultiplier * texColor;
    vec3 result  = ambient;

    for (int i = 0; i < lightCount; i++)
    {
        vec3 lightDir   = normalize(lightPos[i] - fragmentPos);
        vec3 halfwayDir = normalize(lightDir + viewDir);

        float distance    = length(lightPos[i] - fragmentPos);
        float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);

        // Diffuse
        float lambert = max(dot(normal, lightDir), 0.0);
        vec3 diffuse  = lambert * lightColor[i] * lightIntensity[i] * texColor * attenuation;

        // Specular
        float spec    = pow(max(dot(normal, halfwayDir), 0.0), shininess);
        vec3 specular = spec * lightColor[i] * lightIntensity[i] * attenuation;

        result += diffuse + specular;
    }

    // Clamp final result
    result = clamp(result, 0.0, 1.0);

    screenColor = vec4(result, 1.0);
}