#version 330 core
in vec2 fragUV;
out vec4 screenColor;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform vec3  cameraPos;
uniform float ambientMultiplier;

#define MAX_LIGHTS 8
uniform vec3  lightPos[MAX_LIGHTS];
uniform vec3  lightColor[MAX_LIGHTS];
uniform float lightIntensity[MAX_LIGHTS];
uniform int   lightCount;

void main()
{
    vec3  fragPos  = texture(gPosition, fragUV).rgb;
    vec3  normal   = texture(gNormal,   fragUV).rgb;
    vec3  albedo   = texture(gAlbedo,   fragUV).rgb;
    float shine    = texture(gAlbedo,   fragUV).a * 512.0;

    vec3 viewDir = normalize(cameraPos - fragPos);
    vec3 result  = ambientMultiplier * albedo;

    for (int i = 0; i < lightCount; i++) {
        vec3  lightDir    = normalize(lightPos[i] - fragPos);
        vec3  halfwayDir  = normalize(lightDir + viewDir);
        float distance    = length(lightPos[i] - fragPos);
        float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);

        float lambert = max(dot(normal, lightDir), 0.0);
        vec3  diffuse = lambert * lightColor[i] * lightIntensity[i] * albedo * attenuation;

        float spec    = pow(max(dot(normal, halfwayDir), 0.0), shine);
        vec3  specular = spec * lightColor[i] * lightIntensity[i] * attenuation;

        result += diffuse + specular;
    }

    result = clamp(result, 0.0, 1.0);
    screenColor = vec4(result, 1.0);
}