#pragma once
#include "config.h"
#include "particleSystem.h"

class ParticleSystemManager
{
public:
    static ParticleSystemManager& Instance() {
        static ParticleSystemManager instance;
        return instance;
    }

    void Register(std::shared_ptr<ParticleSystem> system)
    {
        systems.push_back(system);
    }

    void Update(float deltaTime) {
        for (auto& system : systems) {
            system->Update(deltaTime);
        }
    }

    void Draw(unsigned int shader, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
        for (auto& system : systems) {
            system->Draw(shader, viewMatrix, projectionMatrix);
        }
    }

    const std::vector<std::shared_ptr<ParticleSystem>>& GetSystems() const { return systems; }

private:
    ParticleSystemManager() = default;
    std::vector<std::shared_ptr<ParticleSystem>> systems;
};