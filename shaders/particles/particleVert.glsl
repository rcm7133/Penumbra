#version 430 core

layout(std430, binding = 0) buffer ParticlesBuffer
{
    struct {
        vec3 position;
        float lifetime;
        vec3 velocity;
        float maxLifetime;
        vec4 color;
        float size;
        float padding[3];
    } particles[];
};

uniform mat4 view;
uniform mat4 projection;

out vec4 vColor;
out float vLifeRatio;
out vec3 vWorldPos;

void main() {
    vec3 pos = particles[gl_VertexID].position;
    float lifetime = particles[gl_VertexID].lifetime;
    float maxLife = particles[gl_VertexID].maxLifetime;

    // Hide dead particles
    if (lifetime <= 0.0 || maxLife <= 0.0)
    {
        gl_Position  = vec4(0.0);
        gl_PointSize = 0.0;
        vColor       = vec4(0.0);
        vLifeRatio   = 0.0;
        return;
    }

    float lifeRatio = lifetime / maxLife;

    vec4 viewPos = view * vec4(pos, 1.0);
    gl_Position = projection * viewPos;

    float size = particles[gl_VertexID].size;
    gl_PointSize = size * (300.0 / -viewPos.z); // Scale with distance

    vColor = particles[gl_VertexID].color;
    vLifeRatio = lifeRatio;
    vWorldPos = pos;
}