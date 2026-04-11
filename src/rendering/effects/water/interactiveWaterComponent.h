#pragma once
#include "component.h"
#include "../../../config.h"
#include "rendering/shaderutils.h"

class InteractiveWaterComponent : public Component
{
public:
    int resolution = 256;
    float waveSpeed = 2.0f;
    float dampening = 0.995f;
    float rippleStrength = 0.5f;
    float fresnelPower = 3.0f;
    float specularStrength = 1.0f;
    glm::vec3 shallowColor = glm::vec3(0.1f, 0.4f, 0.6f);
    glm::vec3 deepColor = glm::vec3(0.0f, 0.1f, 0.3f);

    InteractiveWaterComponent(const int resolution = 256, const float waveSpeed = 2.0f)
        : resolution(resolution), waveSpeed(waveSpeed) {
        InitTextures();
        InitShaders();
    }

    const char* GetTypeName() const override { return "InteractiveWater"; }

    unsigned int GetHeightMap() const { return heightMap; }
    unsigned int GetNormalMap() const { return normalMap; }
    unsigned int GetWaterShader() const { return waterShader; }

private:
    // Textures
    unsigned int heightMap;
    unsigned int normalMap;
    // Shaders
    unsigned int waterShader;
    // Compute shaders
    unsigned int simulateShader = 0;
    unsigned int normalShader = 0;

    void InitTextures() {
        // heightMap
        glGenTextures(1, &heightMap);
        glBindTexture(GL_TEXTURE_2D, heightMap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, resolution, resolution, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // normal map
        glGenTextures(1, &normalMap);
        glBindTexture(GL_TEXTURE_2D, normalMap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resolution, resolution, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    void InitShaders() {
        waterShader = ShaderUtils::MakeShaderProgram("../shaders/water/waterVert.glsl", "../shaders/water/waterFrag.glsl");

    }
};
