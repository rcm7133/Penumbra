#version 430 core
layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gAlbedo;
layout (location = 3) out vec4 gEmissive;

in vec3 fragPos;
in vec2 fragUV;
in vec3 fragNormal;
in mat3 TBN;
in vec3 tangentViewDir;

uniform sampler2D diffuseTex;
uniform sampler2D normalMap;
uniform sampler2D heightMap;
uniform sampler2D metallicRoughnessMap;
uniform bool hasNormalMap;
uniform bool hasHeightMap;
uniform bool hasMetallicRoughnessMap;
uniform float heightScale;

uniform vec3 emissiveColor;
uniform float emissiveIntensity;

uniform float roughness;
uniform float metallic;

vec2 ParallaxOcclusionMap(vec2 uv, vec3 viewDir)
{
    // More layers at grazing angles
    // Fewer layers when looking straight down
    float minLayers = 8.0;
    float maxLayers = 32.0;
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0, 0, 1), viewDir)));

    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;

    // How much to shift UVs per layer
    vec2 P = viewDir.xy / viewDir.z * heightScale;
    vec2 deltaUV = P / numLayers;

    vec2  currentUV = uv;
    float currentHeight = texture(heightMap, currentUV).r;

    // Step through layers until the ray goes below the surface
    while (currentLayerDepth < currentHeight)
    {
        currentUV -= deltaUV;
        currentHeight = texture(heightMap, currentUV).r;
        currentLayerDepth += layerDepth;
    }

    // Interpolate between the last two samples for a precise hit
    vec2 prevUV = currentUV + deltaUV;
    float afterDepth  = currentHeight - currentLayerDepth;
    float beforeDepth = texture(heightMap, prevUV).r - (currentLayerDepth - layerDepth);
    float weight = afterDepth / (afterDepth - beforeDepth);
    return mix(currentUV, prevUV, weight);
}

void main()
{
    if (emissiveIntensity > 0.0)
        gEmissive = vec4(emissiveColor * emissiveIntensity, 1.0);
    else
        gEmissive = vec4(0.0);

    // Apply parallax first
    vec2 uv = fragUV;
    if (hasHeightMap) {
        uv = ParallaxOcclusionMap(fragUV, normalize(tangentViewDir));

        // Discard fragments that parallax pushes outside [0,1]
        if (uv.x > 1.0 || uv.y > 1.0 || uv.x < 0.0 || uv.y < 0.0)
        discard;
    }

    gPosition = vec4(fragPos, 1.0);

    vec3 N = normalize(fragNormal);
    if (hasNormalMap) {
        vec3 mapNormal = texture(normalMap, uv).rgb * 2.0 - 1.0;
        N = normalize(TBN * mapNormal);
    }

    float finalRoughness = roughness;
    float finalMetallic  = metallic;

    if (hasMetallicRoughnessMap) {
        // G = roughness, B = metallic
        vec4 mr = texture(metallicRoughnessMap, uv);
        finalRoughness = mr.g * roughness;
        finalMetallic  = mr.b * metallic;
    }

    gNormal = vec4(N, finalRoughness);

    vec3 albedo = texture(diffuseTex, uv).rgb;
    gAlbedo = vec4(albedo, finalMetallic);
}