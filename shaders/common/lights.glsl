#define MAX_TOTAL_LIGHTS 16
#define MAX_SHADOW_LIGHTS 3

#define LIGHT_DIRECTIONAL 0
#define LIGHT_SPOT        1
#define LIGHT_POINT       2

struct Light {
    vec3 position;
    vec3 color;
    vec3 direction;
    float intensity;
    float innerCutoff;
    float outerCutoff;
    int type;
    float radius;
};

uniform float ambientMultiplier;
uniform float shadowNormalOffset;
uniform float shadowBias;

// PCF
uniform int pcfKernelSize;
uniform int pcfEnabled;
// SSAO
uniform sampler2D ssaoTexture;
uniform bool ssaoEnabled;

// Light Probes / GI
uniform int giMode;           // 0 = off, 1 = light probes
uniform float giIntensity;
uniform vec3 probeGridMin;
uniform vec3 probeGridMax;
uniform ivec3 probeGridCount; // countX, countY, countZ

layout(std430, binding = 1) buffer ProbeData {
    vec4 probeSH[];  // 9 vec4s per probe (xyz = RGB coefficient, w unused)
};

uniform int lightCount;
uniform Light lights[MAX_TOTAL_LIGHTS];

uniform int shadowLightCount;
uniform sampler2D shadowMap[MAX_SHADOW_LIGHTS];
uniform samplerCube shadowCubeMap[MAX_SHADOW_LIGHTS];
uniform mat4 lightSpaceMatrix[MAX_SHADOW_LIGHTS];
uniform float lightFarPlane[MAX_SHADOW_LIGHTS];

float SampleShadowMap(sampler2D map, mat4 lsm, vec3 fragPos, vec3 normal, vec3 toLight)
{
    float nDotL = dot(normal, toLight);
    vec3 offsetPos = fragPos + normal * (shadowNormalOffset * (1.0 - nDotL));
    vec4 fragPosLightSpace = lsm * vec4(offsetPos, 1.0);
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 0.0;

    float currentDepth = proj.z;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(map, 0);

    if (pcfEnabled == 0)
    {
        float closestDepth = texture(map, proj.xy).r;
        return currentDepth - shadowBias > closestDepth ? 1.0 : 0.0;
    }

    int halfKernel = pcfKernelSize / 2;
    int sampleCount = 0;
    for (int x = -halfKernel; x <= halfKernel; x++)
    {
        for (int y = -halfKernel; y <= halfKernel; y++)
        {
            float closestDepth = texture(map, proj.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - shadowBias > closestDepth ? 1.0 : 0.0;
            sampleCount++;
        }
    }
    return shadow / float(sampleCount);
}

float SamplePointShadow(samplerCube cubeMap, vec3 fragPos, vec3 lightPos, float farPlane)
{
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight) / farPlane;
    float closestDepth = texture(cubeMap, fragToLight).r;
    float bias = 0.005;
    return currentDepth - bias > closestDepth ? 1.0 : 0.0;
}

// GI
vec3 EvaluateSH(int probeIndex, vec3 normal)
{
    int base = probeIndex * 9;

    vec3 result = vec3(0);

    // Cosine lobe convolution constants
    const float c0 = 0.886227;
    const float c1 = 1.023328;
    const float c2_a = 0.858086;
    const float c2_b = 0.247708;

    result += c0 * probeSH[base + 0].xyz;
    result += c1 * probeSH[base + 1].xyz * normal.y;
    result += c1 * probeSH[base + 2].xyz * normal.z;
    result += c1 * probeSH[base + 3].xyz * normal.x;
    result += c2_a * probeSH[base + 4].xyz * normal.x * normal.y;
    result += c2_a * probeSH[base + 5].xyz * normal.y * normal.z;
    result += c2_b * probeSH[base + 6].xyz * (3.0 * normal.z * normal.z - 1.0);
    result += c2_a * probeSH[base + 7].xyz * normal.x * normal.z;
    result += c2_a * probeSH[base + 8].xyz * (normal.x * normal.x - normal.y * normal.y);

    return max(result, vec3(0));
}

vec3 SampleProbeGrid(vec3 worldPos, vec3 normal)
{
    // Normalize position into grid space [0, 1]
    vec3 gridPos = (worldPos - probeGridMin) / (probeGridMax - probeGridMin);
    gridPos = clamp(gridPos, vec3(0), vec3(1));

    // Continuous grid indices
    float fx = gridPos.x * float(probeGridCount.x - 1);
    float fy = gridPos.y * float(probeGridCount.y - 1);
    float fz = gridPos.z * float(probeGridCount.z - 1);

    // Lower corner indices
    int x0 = clamp(int(floor(fx)), 0, probeGridCount.x - 2);
    int y0 = clamp(int(floor(fy)), 0, probeGridCount.y - 2);
    int z0 = clamp(int(floor(fz)), 0, probeGridCount.z - 2);

    // Interpolation weights
    float dx = fx - float(x0);
    float dy = fy - float(y0);
    float dz = fz - float(z0);

    // Trilinear interpolation of SH evaluation
    vec3 result = vec3(0);

    for (int cz = 0; cz <= 1; cz++) {
        for (int cy = 0; cy <= 1; cy++) {
            for (int cx = 0; cx <= 1; cx++) {
                int idx = (z0 + cz) * probeGridCount.y * probeGridCount.x
                + (y0 + cy) * probeGridCount.x
                + (x0 + cx);

                float weight = (cx == 1 ? dx : 1.0 - dx)
                * (cy == 1 ? dy : 1.0 - dy)
                * (cz == 1 ? dz : 1.0 - dz);

                result += EvaluateSH(idx, normal) * weight;
            }
        }
    }

    return result;
}