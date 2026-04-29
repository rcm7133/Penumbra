#pragma once
#include "../../../config.h"

class WorleyNoiseGenerator {
public:
    WorleyNoiseGenerator();

    bool inverted = true;

    int resolution = 64;
    int voxelResolutionR = 64;
    int voxelResolutionG = 64;
    int voxelResolutionB = 64;
    int voxelResolutionA = 64;

    GLuint GenerateNoise();
    void SetResolution(int newTexRes, int r, int g, int b, int a);

private:
    // Data buffers
    std::vector<glm::vec4> pointDataR;
    std::vector<glm::vec4> pointDataG;
    std::vector<glm::vec4> pointDataB;
    std::vector<glm::vec4> pointDataA;

    // Min and max xyz values for the 3d tex cube. Should be from 0.0-1.0 for xyz.
    glm::vec3 min = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 max = glm::vec3(1.0f, 1.0f, 1.0f);
    // Compute Shader
    GLuint texturePopulateComputeShader = 0;
    // SSBOs
    GLuint pointRSSBO = 0;
    GLuint pointGSSBO = 0;
    GLuint pointBSSBO = 0;
    GLuint pointASSBO = 0;

    // --- Functions ---

    // Utils
    void CreateBuffers();

    void PopulateRandomPoints(std::vector<glm::vec4>& points, const int voxelResolution);
    int XYZToIndex(const glm::vec3& point, const int voxelResolution);
    glm::vec3 VoxelFloor(const glm::vec3& point, const int voxelResolution);
    glm::vec3 VoxelCeil(const glm::vec3& point, const int voxelResolution);
};