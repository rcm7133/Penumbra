#pragma once
#include "config.h"

struct Material
{
    unsigned int shader;
    unsigned int diffuseTexture;
    float shininess = 32.0f;
    float ambientMultiplier = 0.15f;
    glm::vec3 ambientColor = glm::vec3(1.0f);
};