#pragma once
#define NANORT_IMPLEMENTATION
#include "../../../dependencies/nanoRT/nanort.h"
#include "../../config.h"

struct CPUTexture {
    std::vector<glm::vec3> pixels;
    int width = 0;
    int height = 0;

    glm::vec3 Sample(glm::vec2 uv) const {
        if (pixels.empty()) return glm::vec3(0.8f);
        // wrap
        float u = uv.x - std::floor(uv.x);
        float v = uv.y - std::floor(uv.y);
        int x = (int)(u * width)  % width;
        int y = (int)(v * height) % height;
        return pixels[y * width + x];
    }
};

// Scene to send to pathTracer
struct PTScene {
    nanort::BVHAccel<float> accel;

    std::vector<float> allVerts;
    std::vector<unsigned int> allIndices;
    std::vector<glm::vec2> allUVs;        // per vertex, parallel to allVerts
    std::vector<int> triTextureIndex;     // per triangle, index into textures
    std::vector<CPUTexture> textures;     // one per unique GL handle

    std::vector<glm::vec3> triEmissiveColor;
    std::vector<float> triEmissiveIntensity;

    bool built = false;
};

struct PTLight {
    glm::vec3 position;
    glm::vec3 color;
    float intensity;
    float radius;
    int type; // 0=directional, 1=spot, 2=point
    glm::vec3 direction;
};

glm::vec3 TracePath(
    const PTScene& pt,
    const std::vector<PTLight>& lights,
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    int bounces,
    std::mt19937& rng);

glm::vec3 PathTraceProbe(
    const PTScene& pt,
    const std::vector<PTLight>& lights,
    const glm::vec3& probePos,
    const glm::vec3& direction,
    int samplesPerDir,
    int bounces,
    std::mt19937& rng);


class Scene;
CPUTexture ReadbackTexture(unsigned int glHandle);
PTScene BuildPTScene(std::shared_ptr<Scene> scene);
std::vector<PTLight> ExtractLights(std::shared_ptr<Scene> scene);

