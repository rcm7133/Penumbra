#pragma once
#include "../config.h"
#include "particle.h"

class ParticleSystem
{
public:
    glm::vec3 position = glm::vec3(0, 0, 0);
    int maxParticles;
    glm::vec3 boundsMin = glm::vec3(-2.0f, 0.0f, -2.0f);
    glm::vec3 boundsMax = glm::vec3(2.0f, 4.0f,  2.0f);

    ParticleSystem(glm::vec3 pos, int maxParticles) : position(pos), maxParticles(maxParticles)
    {
        glGenBuffers(1, &particleSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, maxParticles * sizeof(Particle), nullptr, GL_DYNAMIC_DRAW);

        computeShader = LoadComputeShader("../shaders/compute/particles/particleCompute.glsl");
    }

    void Update(const float& deltaTime)
    {
        glUseProgram(computeShader);
        glUniform1f(glGetUniformLocation(computeShader, "deltaTime"), deltaTime);
        glUniform1f(glGetUniformLocation(computeShader, "time"), (float)glfwGetTime());
        glUniform3fv(glGetUniformLocation(computeShader, "emitterPos"), 1, glm::value_ptr(position));
        glUniform3fv(glGetUniformLocation(computeShader, "boundsMin"),  1, glm::value_ptr(boundsMin));
        glUniform3fv(glGetUniformLocation(computeShader, "boundsMax"),  1, glm::value_ptr(boundsMax));

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO);
        int groups = (maxParticles + 63) / 64;
        glDispatchCompute(groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    }

    void Draw(unsigned int shader, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {

    }

private:
    unsigned int particleSSBO;
    unsigned int computeShader;

    unsigned int LoadComputeShader(const std::string& path)
    {
        std::ifstream file(path);
        std::stringstream ss;
        ss << file.rdbuf();
        std::string src = ss.str();
        const char* srcPtr = src.c_str();

        unsigned int shader = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(shader, 1, &srcPtr, NULL);
        glCompileShader(shader);

        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[1024];
            glGetShaderInfoLog(shader, 1024, NULL, log);
            std::cerr << "Compute shader error:\n" << log << std::endl;
        }

        unsigned int program = glCreateProgram();
        glAttachShader(program, shader);
        glLinkProgram(program);
        glDeleteShader(shader);
        return program;
    }
};