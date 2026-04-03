#version 430 core

in vec4 vColor;
in float vLifeRatio;
in vec3 vWorldPos;

out vec4 FragColor;

#define MAX_TOTAL_LIGHTS 16
#define MAX_SHADOW_LIGHTS 3
#define LIGHT_DIRECTIONAL 0
#define LIGHT_SPOT 1

uniform int lightCount;
uniform vec3 lightPos[MAX_TOTAL_LIGHTS];
uniform vec3 lightColor[MAX_TOTAL_LIGHTS];
uniform float lightIntensity[MAX_TOTAL_LIGHTS];
uniform vec3  lightDir[MAX_TOTAL_LIGHTS];
uniform float innerCutoff[MAX_TOTAL_LIGHTS];
uniform float outerCutoff[MAX_TOTAL_LIGHTS];
uniform int   lightType[MAX_TOTAL_LIGHTS];

uniform int shadowLightCount;
uniform sampler2D shadowMap[MAX_SHADOW_LIGHTS];
uniform mat4 lightSpaceMatrix[MAX_SHADOW_LIGHTS];

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

        if (lightType[i] == LIGHT_DIRECTIONAL)
        {
            toLight     = normalize(-lightDir[i]);
            attenuation = 1.0;
        }

        else
        {
            toLight = normalize(lightPos[i] - vWorldPos);
            float d = length(lightPos[i] - vWorldPos);
            attenuation = 1.0 / (1.0 + 0.09 * d + 0.032 * d * d);

            float theta   = dot(toLight, normalize(-lightDir[i]));
            float epsilon = innerCutoff[i] - outerCutoff[i];
            spotEffect    = clamp((theta - outerCutoff[i]) / epsilon, 0.0, 1.0);
        }

        float shadow = 0.0;
        if (i < shadowLightCount)
            shadow = ShadowCalc(i, vWorldPos);

        litResult += lightColor[i] * lightIntensity[i] * attenuation * spotEffect * (1.0 - shadow);
    }

    FragColor = vec4(vColor.rgb * litResult, vColor.a * alpha);
}