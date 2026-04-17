#pragma once
#include "../../config.h"

struct Material
{
    unsigned int shader;
    unsigned int diffuseTexture = 0;
    unsigned int normalMap = 0;
    unsigned int heightMap = 0;
    unsigned int emissiveTexture = 0;
    unsigned int metallicRoughnessMap = 0; // G=roughness, B=metallic

    bool hasNormalMap = false;
    bool hasHeightMap = false;
    bool hasMetallicRoughnessMap = false;
    bool hasEmissiveTexture = false;
    float heightScale = 0.05f;
    float ambientMultiplier = 0.15f;
    glm::vec3 ambientColor = glm::vec3(1.0f);

    // PBR parameters
    float roughness = 0.5f;
    float metallic = 0.0f;

    glm::vec3 emissiveColor = glm::vec3(0.0f);
    float emissiveIntensity = 0.0f;

    std::string modelPath;
    std::string texturePath;
    std::string normalMapPath;
    std::string heightMapPath;
    std::string metallicRoughnessMapPath;
    std::string emissiveTexturePath;
};