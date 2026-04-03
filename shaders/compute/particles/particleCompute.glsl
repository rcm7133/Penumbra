#version 430 core
layout(local_size_x = 64) in;

struct Particle {
    vec3 position;
    float lifetime;
    vec3 velocity;
    float maxLifetime;
    vec4 color;
    float size;
    float padding[3]; // Keep 16 byte alignment
};

layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

uniform float deltaTime;
uniform float time;
uniform vec3 emitterPos;
uniform vec3 boundsMin;
uniform vec3 boundsMax;
uniform vec4 startColor;
uniform vec4 endColor;
uniform float minSize;
uniform float maxSize;
uniform float minLifetime;
uniform float maxLifetime;
uniform float speed;
uniform float fadeInTime;


float hash(uint seed) {
    seed ^= seed >> 16;
    seed *= 0x45d9f3bu;
    seed ^= seed >> 16;
    seed *= 0x45d9f3bu;
    seed ^= seed >> 16;
    return float(seed) / float(0xFFFFFFFFu);
}

void main()
{
    uint id = gl_GlobalInvocationID.x;
    Particle p = particles[id];

    if (p.lifetime <= 0.0)
    {
        p.position = emitterPos + vec3(
        mix(boundsMin.x, boundsMax.x, hash(id * 3u + 0u)),
        mix(boundsMin.y, boundsMax.y, hash(id * 3u + 1u)),
        mix(boundsMin.z, boundsMax.z, hash(id * 3u + 2u))
        );

        p.velocity = vec3(
        (hash(id * 7u + 3u) - 0.5) * 0.5,
        hash(id * 7u + 4u) * 1.0,
        (hash(id * 7u + 5u) - 0.5) * 0.5
        ) * speed;

        p.maxLifetime = mix(minLifetime, maxLifetime, hash(id * 7u + 6u));
        p.lifetime    = p.maxLifetime;
        p.color       = startColor;
        p.size        = mix(minSize, maxSize, hash(id * 11u + 7u));
    }
    else
    {
        p.position += p.velocity * deltaTime;
        p.lifetime -= deltaTime;

        // Lerp color over lifetime
        float lifeRatio = p.lifetime / p.maxLifetime;
        float age = p.maxLifetime - p.lifetime;
        float fadeIn = clamp(age / fadeInTime, 0.0, 1.0);

        p.color = mix(endColor, startColor, lifeRatio);
        p.color.a *= fadeIn;
    }

    particles[id] = p;
}