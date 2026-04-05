#pragma once
#include "config.h"
#include "particleSystem.h"

class ParticleSystemManager
{
public:
    void Register(std::shared_ptr<ParticleSystem> system)
    {
        systems.push_back(system);
    }

    void Update(float deltaTime) {
        for (auto& system : systems) {
            system->Update(deltaTime);
        }
    }

    void Reset() {
        systems.clear();
    }

    const std::vector<std::shared_ptr<ParticleSystem>>& GetSystems() const { return systems; }

private:
    std::vector<std::shared_ptr<ParticleSystem>> systems{};
};