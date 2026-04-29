uniform sampler3D cloudNoiseTex;
uniform vec3 cloudMin;
uniform vec3 cloudMax;
uniform float cloudScale;
uniform float cloudScrollSpeed;
uniform float time;

// Channel weights for Worley noise layers
uniform float rWeight;
uniform float gWeight;
uniform float bWeight;
uniform float aWeight;

float SampleDensity(vec3 worldPos) {
    vec3 uvw = worldPos * cloudScale + vec3(time * cloudScrollSpeed, 0.0, time * cloudScrollSpeed * 0.7);
    vec4 noise = texture(cloudNoiseTex, uvw);

    float density = noise.r * rWeight
    + noise.g * gWeight
    + noise.b * bWeight
    + noise.a * aWeight;

    float totalWeight = rWeight + gWeight + bWeight + aWeight;
    density /= max(totalWeight, 0.0001);

    return max(0.0, density - 0.4) * 2.0;
}