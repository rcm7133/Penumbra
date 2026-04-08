#pragma once
#include "../component.h"
#include "../rendering/fogVolume.h"

class FogVolumeComponent : public Component
{
public:
    std::shared_ptr<FogVolume> volume;

    FogVolumeComponent() : volume(std::make_shared<FogVolume>()) {}

    const char* GetTypeName() const override { return "FogVolume"; }
};