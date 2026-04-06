#version 430 core
#include "../common/lights.glsl"
#include "../common/fog.glsl"

in vec2 fragUV;
out vec4 screenColor;

uniform sampler2D litScene;
uniform sampler2D gPosition;
uniform sampler3D noiseTexture;

uniform vec3  cameraPos;
uniform float time;
// Fog Settings
uniform float fogDensity;
uniform int steps;
uniform float fogScale;
uniform float fogScrollSpeed;

float SampleShadowMap(sampler2D map, mat4 lsm, vec3 fragPos, float bias)
{
    vec4 fragPosLightSpace = lsm * vec4(fragPos, 1.0);
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    proj = proj * 0.5 + 0.5;

    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
    return 1.0;

    float closestDepth = texture(map, proj.xy).r;
    float currentDepth = proj.z;
    return currentDepth - bias > closestDepth ? 1.0 : 0.0;
}

float SamplePointShadow(samplerCube cubeMap, vec3 fragPos, vec3 lightPos, float farPlane)
{
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight) / farPlane;
    float closestDepth = texture(cubeMap, fragToLight).r;
    float bias = 0.005;
    return currentDepth - bias > closestDepth ? 1.0 : 0.0;
}

bool RayAABB(vec3 rayOrigin, vec3 rayDir, vec3 bmin, vec3 bmax,
out float tNear, out float tFar)
{
    vec3 invDir = 1.0 / rayDir;
    vec3 t1 = (bmin - rayOrigin) * invDir;
    vec3 t2 = (bmax - rayOrigin) * invDir;

    vec3 tMin = min(t1, t2);
    vec3 tMax = max(t1, t2);

    tNear = max(max(tMin.x, tMin.y), tMin.z);
    tFar  = min(min(tMax.x, tMax.y), tMax.z);

    tNear = max(tNear, 0.0);
    return tNear < tFar;
}

vec3 ComputeLighting(vec3 samplePos)
{
    vec3 lighting = vec3(0.0);
    for (int i = 0; i < lightCount; i++)
    {
        float atten = 1.0;

        if (lights[i].type == LIGHT_DIRECTIONAL)
        {
            atten = 1.0;
        }
        else if (lights[i].type == LIGHT_POINT)
        {
            float dist = length(lights[i].position - samplePos);
            atten = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
            atten *= 1.0 - smoothstep(lights[i].radius * 0.75, lights[i].radius, dist);
        }
        else // SPOT
        {
            vec3 toLight = lights[i].position - samplePos;
            float dist = length(toLight);
            atten = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

            vec3 lightDir_i = normalize(toLight);
            float theta = dot(lightDir_i, normalize(-lights[i].direction));
            float epsilon = lights[i].innerCutoff - lights[i].outerCutoff;
            float spotEffect = clamp((theta - lights[i].outerCutoff) / epsilon, 0.0, 1.0);
            atten *= spotEffect;
        }

        float shadow = 0.0;
        if (i < shadowLightCount)
        {
            if (lights[i].type == LIGHT_POINT)
            shadow = SamplePointShadow(shadowCubeMap[i], samplePos, lights[i].position, lightFarPlane[i]);
            else
            shadow = SampleShadowMap(shadowMap[i], lightSpaceMatrix[i], samplePos, 0.0001);
        }

        lighting += lights[i].color * lights[i].intensity * atten * (1.0 - shadow);
    }
    return lighting;
}

vec3 Raymarch(vec3 rayStart, vec3 rayEnd, out float finalTransmittance)
{
    vec3 fogAccum = vec3(0.0);
    float transmittance = 1.0;

    vec3 rayDir = normalize(rayEnd - rayStart);
    float maxDist = length(rayEnd - rayStart);

    for (int v = 0; v < fogVolumeCount; v++)
    {
        float tNear, tFar;
        if (!RayAABB(rayStart, rayDir, fogVolumes[v].boundsMin, fogVolumes[v].boundsMax, tNear, tFar))
        continue;

        tFar = min(tFar, maxDist);
        if (tNear >= tFar) continue;

        float stepSize = (tFar - tNear) / float(steps);

        for (int s = 0; s < steps; s++)
        {
            float t = tNear + (float(s) + 0.5) * stepSize;
            vec3 samplePos = rayStart + rayDir * t;

            float noise = texture(noiseTexture, samplePos * fogVolumes[v].scale
            + time * fogVolumes[v].scrollSpeed).r;
            float density = fogVolumes[v].density * noise;

            vec3 lighting = ComputeLighting(samplePos);

            float extinction = exp(-density * stepSize);
            fogAccum += transmittance * (1.0 - extinction) * lighting;
            transmittance *= extinction;

            if (transmittance < 0.01) break;
        }

        if (transmittance < 0.01) break;
    }

    finalTransmittance = transmittance;
    return fogAccum;
}

void main()
{
    vec3 sceneColor = texture(litScene, fragUV).rgb;
    vec3 fragPos = texture(gPosition, fragUV).rgb;

    if (fragPos == vec3(0.0))
    {
        screenColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float transmittance;
    vec3 fogLight = Raymarch(cameraPos, fragPos, transmittance);
    screenColor = vec4(fogLight, transmittance);
}