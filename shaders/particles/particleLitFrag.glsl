#version 430 core
#include "../common/lights.glsl"
in vec4 vColor;
in float vLifeRatio;
in vec3 vWorldPos;

out vec4 FragColor;

float ShadowCalc(int idx, vec3 worldPos)
{
    vec4 lsPos = lightSpaceMatrix[idx] * vec4(worldPos, 1.0);
    vec3 proj  = lsPos.xyz / lsPos.w;
    proj = proj * 0.5 + 0.5;

    if (proj.z > 1.0) return 0.0;

    float closest = texture(shadowMap[idx], proj.xy).r;
    float current = proj.z;
    return current - 0.005 > closest ? 1.0 : 0.0;
}

float SamplePointShadow(samplerCube cubeMap, vec3 fragPos, vec3 lightPos, float farPlane)
{
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight) / farPlane;
    float closestDepth = texture(cubeMap, fragToLight).r;
    float bias = 0.005;
    return currentDepth - bias > closestDepth ? 1.0 : 0.0;
}

void main() {
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);

    if (dist > 0.5)
    discard;

    float alpha = smoothstep(0.5, 0.0, dist);
    alpha *= vLifeRatio;

    vec3 litResult = vec3(0.01);

    for (int i = 0; i < lightCount; i++)
    {
        vec3 toLight;
        float attenuation = 1.0;
        float spotEffect = 1.0;

        if (lights[i].type == LIGHT_DIRECTIONAL)
        {
            toLight  = normalize(-lights[i].direction);
        }
        else if (lights[i].type == LIGHT_POINT)
        {
            toLight = normalize(lights[i].position - vWorldPos);
            float d = length(lights[i].position - vWorldPos);
            attenuation = 1.0 / (1.0 + 0.09 * d + 0.032 * d * d);
            attenuation *= 1.0 - smoothstep(lights[i].radius * 0.75, lights[i].radius, d);
        }
        else // SPOT
        {
            toLight = normalize(lights[i].position - vWorldPos);
            float d = length(lights[i].position - vWorldPos);
            attenuation = 1.0 / (1.0 + 0.09 * d + 0.032 * d * d);

            float theta   = dot(toLight, normalize(-lights[i].direction));
            float epsilon = lights[i].innerCutoff - lights[i].outerCutoff;
            spotEffect    = clamp((theta - lights[i].outerCutoff) / epsilon, 0.0, 1.0);
        }

        float shadow = 0.0;
        if (i < shadowLightCount)
        {
            if (lights[i].type == LIGHT_POINT)
            shadow = SamplePointShadow(shadowCubeMap[i], vWorldPos, lights[i].position, lightFarPlane[i]);
            else
            shadow = ShadowCalc(i, vWorldPos);
        }

        litResult += lights[i].color * lights[i].intensity * attenuation * spotEffect * (1.0 - shadow);
    }

    FragColor = vec4(vColor.rgb * litResult, vColor.a * alpha);
}