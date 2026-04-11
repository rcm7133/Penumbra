#pragma once
#include "../../../config.h"

class InteractiveWater {
public:
    int resolution = 256;
    float waveSpeed = 2.0f;
    float dampening = 0.995f;
    float rippleStrength = 0.5f;
    float fresnelPower = 3.0f;
    float specularStrength = 1.0f;

    glm::vec3 shallowColor = glm::vec3(0.1f, 0.4f, 0.6f);
    glm::vec3 deepColor = glm::vec3(0.0f, 0.1f, 0.3f);

    InteractiveWater(const int resolution = 256, const float waveSpeed = 2.0f) : resolution(resolution), waveSpeed(waveSpeed) {}

    // Textures
    unsigned int heightMap;
    unsigned int normalMap;
    // Shaders
    unsigned int waterShader;
    // Compute shaders
    unsigned int simulateShader = 0;
    unsigned int normalShader = 0;
};