#version 430 core

in vec2 fragUV;
out vec4 screenColor;

#define MAX_TOTAL_LIGHTS 16
#define MAX_SHADOW_LIGHTS 3
#define LIGHT_DIRECTIONAL 0
#define LIGHT_SPOT        1

uniform sampler2D litScene;
uniform sampler2D gPosition;
uniform sampler3D noiseTexture;

uniform int lightCount;
uniform vec3 lightPos[MAX_TOTAL_LIGHTS];
uniform vec3 lightColor[MAX_TOTAL_LIGHTS];
uniform float lightIntensity[MAX_TOTAL_LIGHTS];
uniform vec3 lightDir[MAX_TOTAL_LIGHTS];
uniform int lightType[MAX_TOTAL_LIGHTS];

uniform int shadowLightCount;
uniform sampler2D shadowMap[MAX_SHADOW_LIGHTS];
uniform mat4 lightSpaceMatrix[MAX_SHADOW_LIGHTS];

uniform float innerCutoff[MAX_TOTAL_LIGHTS];
uniform float outerCutoff[MAX_TOTAL_LIGHTS];

uniform vec3  cameraPos;
uniform float time;
// Fog Settings
uniform float fogDensity;
uniform int steps;
uniform float fogScale;
uniform float fogScrollSpeed;

// Sample realtime shadow map
float SampleShadowMap(sampler2D map, mat4 lsm, vec3 fragPos, float bias)
{
    vec4 fragPosLightSpace = lsm * vec4(fragPos, 1.0);
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    proj = proj * 0.5 + 0.5;

    // Outside frustum treat as shadowed
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
    return 1.0;

    float closestDepth = texture(map, proj.xy).r;
    float currentDepth = proj.z;
    return currentDepth - bias > closestDepth ? 1.0 : 0.0;
}

vec3 Raymarch(vec3 rayStart, vec3 rayEnd, out float finalTransmittance)
{
    finalTransmittance = 1.0;
    vec3 rayDir = rayEnd - rayStart;
    float rayLength = length(rayDir);

    vec3 stepVec = rayDir / float(steps);
    float stepSize = rayLength / float(steps);

    vec3 fogAccum = vec3(0.0);
    float transmittance = 1.0;

    for (int s = 0; s < steps; s++)
    {
        vec3 samplePos = rayStart + stepVec * (float(s) + 0.5);
        float noise = texture(noiseTexture, samplePos * fogScale + time * fogScrollSpeed).r;
        float density = fogDensity * noise;

        // Accumulate light contribution at this sample point
        vec3 lighting = vec3(0.0);
        for (int i = 0; i < lightCount; i++)
        {
            float atten = 1.0;
            float spotEffect = 1.0;

            if (lightType[i] == LIGHT_DIRECTIONAL)
            {
                atten = 1.0;
                spotEffect = 1.0;
            }
            else // Spotlight
            {
                vec3 toLight = lightPos[i] - samplePos;
                float dist = length(toLight);
                atten = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

                vec3 lightDir_i = normalize(toLight);
                float theta = dot(lightDir_i, normalize(-lightDir[i]));
                float epsilon = innerCutoff[i] - outerCutoff[i];
                spotEffect = clamp((theta - outerCutoff[i]) / epsilon, 0.0, 1.0);
                atten *= spotEffect;
            }

            float shadow = 0.0;
            if (i < shadowLightCount)
            shadow = SampleShadowMap(shadowMap[i], lightSpaceMatrix[i], samplePos, 0.0001);

            lighting += lightColor[i] * lightIntensity[i] * atten * (1.0 - shadow);
        }

        float extinction = exp(-density * stepSize);
        fogAccum += transmittance * (1.0 - extinction) * lighting;
        transmittance *= extinction;
        finalTransmittance = transmittance;

        if (transmittance < 0.01) break;
    }
    return fogAccum;
}

void main()
{
    vec3 sceneColor = texture(litScene, fragUV).rgb;
    vec3 fragPos = texture(gPosition, fragUV).rgb;

    // No geometry, full fog
    if (fragPos == vec3(0.0))
    {
        screenColor = vec4(0.0, 0.0, 0.0, 1.0); // no fog light, full transmittance
        return;
    }

    float transmittance;
    vec3 fogLight = Raymarch(cameraPos, fragPos, transmittance);
    screenColor = vec4(fogLight, transmittance);
}