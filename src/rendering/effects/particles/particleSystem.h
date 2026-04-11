#pragma once
#include "../../../config.h"
#include "particle.h"
#include "../../shaderutils.h"


class ParticleSystem
{
public:
    bool lit = true;
    int maxParticles;
    glm::vec3 boundsMin = glm::vec3(-2.0f, 0.0f, -2.0f);
    glm::vec3 boundsMax = glm::vec3(2.0f, 4.0f,  2.0f);
    glm::vec4 startColor = glm::vec4(1.0f, 0.6f, 0.3f, 1.0f);
    glm::vec4 endColor   = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    float minSize = 5.0f;
    float maxSize = 10.0f;
    float minLifetime = 2.0f;
    float maxLifetime = 5.0f;
    float fadeInTime = 1.0f;
    float speed = 1.0f;

    ParticleSystem(int maxParticles, bool lit = true) : maxParticles(maxParticles), lit(lit)
    {
        glGenVertexArrays(1, &dummyVAO);
        glGenBuffers(1, &particleSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, maxParticles * sizeof(Particle), nullptr, GL_DYNAMIC_DRAW);
        computeShader = ShaderUtils::LoadComputeShader("../shaders/compute/particles/particleCompute.glsl");
    }

    void Update(const float& deltaTime, const glm::vec3& transformPos)
    {
        glm::vec3 worldBoundsMin = boundsMin + transformPos;
        glm::vec3 worldBoundsMax = boundsMax + transformPos;

        glUseProgram(computeShader);
        glUniform1f(glGetUniformLocation(computeShader, "deltaTime"), deltaTime);
        glUniform1f(glGetUniformLocation(computeShader, "time"), (float)glfwGetTime());
        glUniform3fv(glGetUniformLocation(computeShader, "boundsMin"), 1, glm::value_ptr(worldBoundsMin));
        glUniform3fv(glGetUniformLocation(computeShader, "boundsMax"), 1, glm::value_ptr(worldBoundsMax));

        glUniform4fv(glGetUniformLocation(computeShader, "startColor"), 1, glm::value_ptr(startColor));
        glUniform4fv(glGetUniformLocation(computeShader, "endColor"),   1, glm::value_ptr(endColor));
        glUniform1f(glGetUniformLocation(computeShader, "minSize"),     minSize);
        glUniform1f(glGetUniformLocation(computeShader, "maxSize"),     maxSize);
        glUniform1f(glGetUniformLocation(computeShader, "minLifetime"), minLifetime);
        glUniform1f(glGetUniformLocation(computeShader, "maxLifetime"), maxLifetime);
        glUniform1f(glGetUniformLocation(computeShader, "speed"),       speed);
        glUniform1f(glGetUniformLocation(computeShader, "fadeInTime"), fadeInTime);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO);
        int groups = (maxParticles + 63) / 64;
        glDispatchCompute(groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    }

    unsigned int GetSSBO() const { return particleSSBO; }
    unsigned int GetDummyVAO() const { return dummyVAO; }
    int GetCount() const { return maxParticles; }
    bool IsLit() const { return lit; }

private:
    unsigned int particleSSBO = 0;
    unsigned int computeShader = 0;
    unsigned int dummyVAO = 0;
};