#version 330 core
in vec2 fragUV;
out vec4 screenColor;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform vec3  cameraPos;
uniform float ambientMultiplier;

#define MAX_TOTAL_LIGHTS 16
#define MAX_SHADOW_LIGHTS 3

#define LIGHT_DIRECTIONAL 0
#define LIGHT_SPOT        1

// All lights
uniform int lightCount;
uniform vec3 lightPos[MAX_TOTAL_LIGHTS];
uniform vec3 lightColor[MAX_TOTAL_LIGHTS];
uniform float lightIntensity[MAX_TOTAL_LIGHTS];
uniform vec3  lightDir[MAX_TOTAL_LIGHTS];
uniform float innerCutoff[MAX_TOTAL_LIGHTS];
uniform float outerCutoff[MAX_TOTAL_LIGHTS];
uniform int   lightType[MAX_TOTAL_LIGHTS];
// Realtime Shadow Lights
uniform int shadowLightCount;
uniform sampler2D shadowMap[MAX_SHADOW_LIGHTS];
uniform mat4 lightSpaceMatrix[MAX_SHADOW_LIGHTS];

uniform int pcfKernelSize;
uniform int pcfEnabled;

// Sample realtime shadow map
float SampleShadowMap(sampler2D map, mat4 lsm, vec3 fragPos, float bias)
{
    vec4 fragPosLightSpace = lsm * vec4(fragPos, 1.0);
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 0.0;

    float currentDepth = proj.z;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(map, 0);

    if (pcfEnabled == 0)
    {
        float closestDepth = texture(map, proj.xy).r;
        return currentDepth - bias > closestDepth ? 1.0 : 0.0;
    }

    // Sample a kernel and average the closest depth
    // Percentage Closer Filtering
    int halfKernel = pcfKernelSize / 2;
    int sampleCount = 0;
    for (int x = -halfKernel; x <= halfKernel; x++)
    {
        for (int y = -halfKernel; y <= halfKernel; y++)
        {
            float closestDepth = texture(map, proj.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > closestDepth ? 1.0 : 0.0;
            sampleCount++;
        }
    }

    return shadow / float(sampleCount);
}

void main()
{
    vec3 fragPos = texture(gPosition, fragUV).rgb;
    vec3 normal  = texture(gNormal,   fragUV).rgb;
    vec3 albedo  = texture(gAlbedo,   fragUV).rgb;
    float shine  = texture(gAlbedo,   fragUV).a * 512.0;

    vec3 viewDir = normalize(cameraPos - fragPos);
    vec3 result  = ambientMultiplier * albedo;

    for (int i = 0; i < lightCount; i++)
    {
        vec3  lightDir_i;
        float attenuation = 1.0;
        float spotEffect  = 1.0;

        if (lightType[i] == LIGHT_DIRECTIONAL)
        {
            lightDir_i  = normalize(-lightDir[i]);
            attenuation = 1.0;
        }
        else // SPOT
        {
            lightDir_i = normalize(lightPos[i] - fragPos);
            float dist = length(lightPos[i] - fragPos);
            // Quadradic attenuation curve (inverse square)
            // https://wiki.ogre3d.org/tiki-index.php?page=-Point+Light+Attenuation
            attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

            float theta = dot(lightDir_i, normalize(-lightDir[i]));
            float epsilon = innerCutoff[i] - outerCutoff[i];
            spotEffect = clamp((theta - outerCutoff[i]) / epsilon, 0.0, 1.0);
        }

        vec3  halfwayDir = normalize(lightDir_i + viewDir);
        float lambert = max(dot(normal, lightDir_i), 0.0);
        vec3  diffuse = lambert * lightColor[i] * lightIntensity[i] * albedo * attenuation * spotEffect;

        float spec = pow(max(dot(normal, halfwayDir), 0.0), shine);
        vec3  specular = spec * lightColor[i] * lightIntensity[i] * attenuation * spotEffect;

        float shadow = 0.0;
        if (i < shadowLightCount)
        {
            float bias = max(0.001 * (1.0 - dot(normal, lightDir_i)), 0.005);
            shadow = SampleShadowMap(shadowMap[i], lightSpaceMatrix[i], fragPos, bias);
        }

        result += (1.0 - shadow) * (diffuse + specular);
    }

    result = clamp(result, 0.0, 1.0);
    screenColor = vec4(result, 1.0);
}