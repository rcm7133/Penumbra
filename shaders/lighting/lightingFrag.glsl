#version 430 core
#include "../common/lights.glsl"

const float PI = 3.14159265359;

in vec2 fragUV;
out vec4 screenColor;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D gEmissive;
uniform vec3  cameraPos;

//---------------------------
// PBR Functions
//---------------------------

// Normal Distribution Function (GGX)
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float NdotH = max(dot(N,H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a - 1.0) + 1.0;
    denom = PI * denom * denom;

    return a / max(denom, 0.0001);
}

// Geometry Function Schlick-GGX
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

// Smith's method, combines masking and shadowing
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// Fresnel effect
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}


void main()
{
    vec3 fragPos = texture(gPosition, fragUV).rgb;
    vec3 N = normalize(texture(gNormal, fragUV).rgb);
    float roughness = texture(gNormal, fragUV).a;
    vec3 albedo = texture(gAlbedo, fragUV).rgb;
    float metallic = texture(gAlbedo, fragUV).a;

    // view direction
    vec3 V = normalize(cameraPos - fragPos);

    // F0 = reflectance at normal incidence
    // Dielectrics reflect about 4% gray, metals reflect their albedo color
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec2 screenUV = gl_FragCoord.xy / vec2(textureSize(gPosition, 0));
    float ao = ssaoEnabled ? texture(ssaoTexture, screenUV).r : 1.0;

    float isStatic = texture(gEmissive, fragUV).a;

    // Ambient / GI
    vec3 ambient;
    if (giMode >= 1 && isStatic > 0.5) {
        vec3 indirect = SampleProbeGrid(fragPos, N);
        // Probes capture full irradiance including albedo of surrounding surfaces,
        // but we still need to modulate by this surface's albedo
        ambient = indirect * albedo * giIntensity * ao;
    } else {
        // Dynamic objects (or GI off) use flat ambient
        ambient = ambientMultiplier * albedo * ao;
    }

    // Emission
    vec3 Lo = texture(gEmissive, fragUV).rgb;

    for (int i = 0; i < lightCount; i++)
    {
        vec3  L;
        float attenuation = 1.0;
        float spotEffect  = 1.0;

        if (lights[i].type == LIGHT_DIRECTIONAL)
        {
            L = normalize(-lights[i].direction);
        }
        else if (lights[i].type == LIGHT_POINT)
        {
            L = normalize(lights[i].position - fragPos);
            float dist = length(lights[i].position - fragPos);
            attenuation = 1.0 / (dist * dist);  // inverse square
            attenuation *= 1.0 - smoothstep(lights[i].radius * 0.75, lights[i].radius, dist);
        }
        else // SPOT
        {
            L = normalize(lights[i].position - fragPos);
            float dist = length(lights[i].position - fragPos);
            attenuation = 1.0 / (dist * dist);

            float theta = dot(L, normalize(-lights[i].direction));
            float epsilon = lights[i].innerCutoff - lights[i].outerCutoff;
            spotEffect = clamp((theta - lights[i].outerCutoff) / epsilon, 0.0, 1.0);
        }

        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);

        // Radiance arriving from this light
        vec3 radiance = lights[i].color * lights[i].intensity * attenuation * spotEffect;

        // Cook-Torrance specular
        float D = DistributionGGX(N, H, roughness);
        vec3  F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        float G = GeometrySmith(N, V, L, roughness);

        float NdotV = max(dot(N, V), 0.001);
        vec3 numerator = D * F * G;
        float denominator = 4.0 * NdotV * NdotL + 0.0001;
        vec3 specular = numerator / denominator;

        // Diffuse
        // kS = Fresnel = fraction of light that reflects
        // kD = fraction that refracts
        vec3 kS = F;
        vec3 kD = (1.0 - kS) * (1.0 - metallic);

        vec3 diffuse = kD * albedo / PI;

        // Shadows
        float shadow = 0.0;
        if (i < shadowLightCount)
        {
            if (lights[i].type == LIGHT_POINT)
            shadow = SamplePointShadow(shadowCubeMap[i], fragPos, lights[i].position, lightFarPlane[i]);
            else
            shadow = SampleShadowMap(shadowMap[i], lightSpaceMatrix[i], fragPos, N, L);
        }
        // Accumulate
        Lo += (1.0 - shadow) * (diffuse + specular) * radiance * NdotL;
    }

    vec3 color = ambient + Lo;

    // HDR tonemapping (Reinhard)
    color = color / (color + vec3(1.0));
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    screenColor = vec4(color, 1.0);
}