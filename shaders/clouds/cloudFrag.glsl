#version 430 core
#include "../common/clouds.glsl"

uniform sampler3D voxelLightTex;
uniform sampler2D gPosition;

in vec2 TexCoords;
out vec4 FragColor;

uniform vec3 cameraPos;
uniform mat4 invViewProj;
uniform int cloudSteps;
uniform float cloudAbsorption;
uniform vec3 lightColor;
uniform float lightIntensity;


vec3 SampleVoxelLight(vec3 worldPos) {
    vec3 uvw = (worldPos - cloudMin) / (cloudMax - cloudMin);
    // If outside volume return full light
    if (any(lessThan(uvw, vec3(0.0))) || any(greaterThan(uvw, vec3(1.0))))
    {
        return lightColor * lightIntensity;
    }
    return texture(voxelLightTex, uvw).rgb;
}

bool RayAABB(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax, out float tEnter, out float tExit) {
    vec3 t0 = (bmin - ro) / rd;
    vec3 t1 = (bmax - ro) / rd;
    vec3 tMin = min(t0, t1);
    vec3 tMax = max(t0, t1);
    tEnter = max(max(tMin.x, tMin.y), tMin.z);
    tExit  = min(min(tMax.x, tMax.y), tMax.z);
    return tExit > max(tEnter, 0.0);
}

void main()
{
    // Reconstruct ray direction from NDC
    vec4 ndcNear = vec4(TexCoords * 2.0 - 1.0, -1.0, 1.0);
    vec4 ndcFar  = vec4(TexCoords * 2.0 - 1.0,  1.0, 1.0);

    vec4 worldNear = invViewProj * ndcNear;
    vec4 worldFar  = invViewProj * ndcFar;

    worldNear /= worldNear.w;
    worldFar /= worldFar.w;

    vec3 rayDir = normalize(worldFar.xyz - worldNear.xyz);
    vec3 rayOrigin = cameraPos;

    // Geometry depth cutoff
    vec3 gPos = texture(gPosition, TexCoords).rgb;
    float geomDist = (gPos == vec3(0.0)) ? 1e10 : length(gPos - cameraPos);

    float tEnter, tExit;
    if (!RayAABB(rayOrigin, rayDir, cloudMin, cloudMax, tEnter, tExit)) {
        FragColor = vec4(0.0);
        return;
    }

    tExit = min(tExit, geomDist);
    if (tEnter >= tExit) {
        FragColor = vec4(0.0);
        return;
    }

    float stepSize = (tExit - tEnter) / float(cloudSteps);
    float transmittance = 1.0;
    vec3 scatteredLight = vec3(0.0);

    for (int i = 0; i < cloudSteps; i++)
    {
        float t = tEnter + (float(i) + 0.5) * stepSize;
        vec3 p = rayOrigin + rayDir * t;

        float density = SampleDensity(p);
        if (density < 0.001) continue;

        transmittance *= exp(-density * stepSize * cloudAbsorption);

        vec3 luminance = SampleVoxelLight(p);
        scatteredLight += luminance * density * stepSize * transmittance;

        if (transmittance < 0.01) break;
    }

    float alpha = 1.0 - transmittance;
    FragColor = vec4(scatteredLight, alpha);
}
