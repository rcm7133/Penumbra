#pragma once
#include "config.h"

enum class LightType {
    Directional,
    Point,
    Spot
};

struct Light
{
    LightType type = LightType::Spot;

    glm::vec3 color     = glm::vec3(1.0f);
    float intensity     = 1.0f;

    bool castsShadow    = false;

    unsigned int shadowFBO   = 0;
    unsigned int shadowMap   = 0;

    glm::mat4 lightSpaceMatrix;
};