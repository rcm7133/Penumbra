#pragma once
#include "config.h"

struct Material
{
    unsigned int shader;
    unsigned int diffuseTexture;
    unsigned int normalMap = 0;
    bool hasNormalMap = false;
    float shininess = 32.0f;
    float ambientMultiplier = 0.15f;
    glm::vec3 ambientColor = glm::vec3(1.0f);

    std::string modelPath;
    std::string texturePath;
    std::string normalMapPath;
};