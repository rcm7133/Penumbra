#pragma once
#include "../config.h"

enum class LightType {
    Directional,
    Spot,
    Point
};

struct Light
{
    LightType type = LightType::Spot;

    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;

    glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);

    // Spotlight only
    float innerCutoff   = glm::cos(glm::radians(12.5f)); //cosine
    float outerCutoff   = glm::cos(glm::radians(35.5f));

    bool castsShadow = false;

    unsigned int shadowFBO = 0;
    unsigned int shadowMap = 0;

    glm::mat4 lightSpaceMatrix = glm::mat4(1.0f);

    unsigned int shadowCubemap = 0;
    unsigned int shadowCubeFBO = 0;
    unsigned int shadowCubeDepthRBO = 0;

    float radius = 5.0f;
};