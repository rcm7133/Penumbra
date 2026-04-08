#pragma once
#include "../component.h"
#include "particleSystem.h"

class ParticleSystemComponent : public Component
{
public:
    std::shared_ptr<ParticleSystem> system;

    ParticleSystemComponent(int maxParticles = 10000, bool lit = true)
        : system(std::make_shared<ParticleSystem>(maxParticles, lit)) {}

    const char* GetTypeName() const override { return "ParticleSystem"; }

    void Update(float deltaTime) override
    {
        if (system)
            system->Update(deltaTime, owner->transform.position);
    }
};