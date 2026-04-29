#pragma once
#include "../../../config.h"
#include "worleyNoise.h"


class CloudVolume {
public:
    // General Settings
    glm::vec3 min = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 max = glm::vec3(0.0f, 0.0f, 0.0f);
    float scrollSpeed = 0.1f;
    float scale = 0.5f;
    // Weights
    float rWeight = 1.0f;
    float gWeight = 1.0f;
    float bWeight = 1.0f;
    float aWeight = 1.0f;
    // Noise Generation
    WorleyNoiseGenerator noiseGenerator;
    GLuint noiseTex = 0;
    // Lighting
    glm::ivec3 lightingVoxelGridSize = glm::ivec3(64, 32, 64);
    GLuint voxelLightTexture = 0;

    // Functions
    CloudVolume() {}
    void GenerateNoise() { noiseTex = noiseGenerator.GenerateNoise(); }

    void InitializeVoxelGrid() {
        glGenTextures(1, &voxelLightTexture);
        glBindTexture(GL_TEXTURE_3D, voxelLightTexture);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, lightingVoxelGridSize.x, lightingVoxelGridSize.y, lightingVoxelGridSize.z, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_3D, 0);
    }

    ~CloudVolume() {
        if (noiseTex) glDeleteTextures(1, &noiseTex);
        if (voxelLightTexture) glDeleteTextures(1, &voxelLightTexture);
    }
};