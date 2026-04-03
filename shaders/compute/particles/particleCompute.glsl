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

float hash(float seed) {
    return fract(sin(seed * 127.1 + time) * 43758.5);
}

void main()
{
    uint id = gl_GlobalInvocationID.x;
    Particle p = particles[id];

    if (p.lifetime <= 0.0)
    {
        // Respawn
        p.position = emitterPos  + vec3(
            mix(boundsMin.x, boundsMax.x, hash(float(id) * 1.1)),
            mix(boundsMin.y, boundsMax.y, hash(float(id) * 2.3)),
            mix(boundsMin.z, boundsMax.z, hash(float(id) * 3.7))
        );

        p.velocity = vec3(
            (hash(float(id) * 4.1) - 0.5) * 0.5,
            hash(float(id) * 5.2) * 1.0,
            (hash(float(id) * 6.3) - 0.5) * 0.5
        );
        p.lifetime = p.maxLifetime;
    }

    else
    {
        p.position += p.velocity * deltaTime;
        p.lifetime -= deltaTime;
        // Kill any outside bounds
        if (any(lessThan(p.position, boundsMin)) || any(greaterThan(p.position, boundsMax)))
        {
            p.lifetime = 0.0;
        }
    }

    particles[id] = p;
}