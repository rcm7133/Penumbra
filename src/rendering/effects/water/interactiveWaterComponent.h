#pragma once
#include "component.h"
#include "../../../config.h"
#include "rendering/shaderutils.h"
#include "interactiveWater.h"

class InteractiveWaterComponent : public Component
{
public:
    std::shared_ptr<InteractiveWater> interactiveWater;

    InteractiveWaterComponent(const int resolution = 1024) {
        interactiveWater = std::make_shared<InteractiveWater>(resolution);
    }

    const char* GetTypeName() const override { return "InteractiveWater"; }

    unsigned int GetHeightMap() const { return interactiveWater->GetHeightMap(); }
    unsigned int GetNormalMap() const { return interactiveWater->GetNormalMap(); }
    unsigned int GetWaterShader() const { return interactiveWater->GetWaterShader(); }

};
