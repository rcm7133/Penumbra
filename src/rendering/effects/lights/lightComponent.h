#pragma once
#include "../../../component.h"
#include "light.h"

class LightComponent : public Component
{
public:
    std::shared_ptr<Light> light;

    LightComponent() : light(std::make_shared<Light>()) {}

    LightComponent(std::shared_ptr<Light> existing)
        : light(std::move(existing)) {}

    const char* GetTypeName() const override { return "Light"; }
};