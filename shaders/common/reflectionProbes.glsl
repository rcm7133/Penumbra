uniform samplerCube reflectionProbes[8];
uniform vec3 probeBoundsMin[8];
uniform vec3 probeBoundsMax[8];
uniform vec3 probePositions[8];
uniform float probeBlendRadius[8];
uniform int probeCount;

uniform samplerCube skybox;
uniform bool hasSkybox;

vec3 BoxProjectReflection(vec3 reflDir, vec3 fragPos, vec3 probePos, vec3 bMin, vec3 bMax)
{
    // Intersect from fragPos along reflDir with the AABB
    vec3 invDir = 1.0 / reflDir;

    vec3 tMin = (bMin - fragPos) * invDir;
    vec3 tMax = (bMax - fragPos) * invDir;

    // For each axis get the far intersection
    vec3 tFar = max(tMin, tMax);

    // Smallest far intersection = exit point
    float t = min(min(tFar.x, tFar.y), tFar.z);

    // Only use positive t (ray goes forward)
    t = max(t, 0.0);

    vec3 hitPos = fragPos + reflDir * t;
    return normalize(hitPos - probePos);
}


float ProbeWeight(vec3 fragPos, vec3 bMin, vec3 bMax, float blendRadius)
{
    vec3 distances = min(fragPos - bMin, bMax - fragPos);
    float minDist = min(min(distances.x, distances.y), distances.z);
    return smoothstep(0.0, blendRadius, minDist);
}

vec3 SampleReflectionProbes(vec3 reflDir, vec3 fragPos, float roughness)
{
    float lod = roughness * 6.0;

    for (int i = 0; i < probeCount; i++)
    {
        if (any(lessThan(fragPos, probeBoundsMin[i])) ||
        any(greaterThan(fragPos, probeBoundsMax[i])))
        continue;

        // Force probe to bounds center
        vec3 forcedProbePos = (probeBoundsMin[i] + probeBoundsMax[i]) * 0.5;

        vec3 sampleDir = BoxProjectReflection(reflDir, fragPos, forcedProbePos,
        probeBoundsMin[i], probeBoundsMax[i]);
        return textureLod(reflectionProbes[i], sampleDir, lod).rgb;
    }

    if (hasSkybox)
    return textureLod(skybox, reflDir, lod).rgb;

    return vec3(0.0);
}