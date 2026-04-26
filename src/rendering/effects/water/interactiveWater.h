#pragma once
#include "../../../config.h"

class InteractiveWater {
public:
    int resolution = 1024;
    float waveSpeed = 0.5f;
    float dampening = 0.9995f;
    float rippleStrength = 0.5f;
    float fresnelPower = 3.0f;
    float specularStrength = 1.0f;
    glm::vec3 shallowColor = glm::vec3(0.1f, 0.4f, 0.6f);
    glm::vec3 deepColor = glm::vec3(0.0f, 0.1f, 0.3f);

    float rippleTimer = 0.0f;
    float rippleInterval = 0.5f;

    InteractiveWater(const int resolution = 1024)
        : resolution(resolution) { Init(); }

    unsigned int GetHeightMap() const;
    unsigned int GetWaterShader() const { return waterShader; }
    unsigned int GetNormalMap()   const { return normalMap; }

    void ComputeHeightMap();
    void ComputeNormals();
    void AddRippleWorld(glm::vec3 worldPos, glm::vec3 waterWorldPos, float waterSize);

    unsigned int GetHeightMapPing() const { return heightMapPing; }
    unsigned int GetHeightMapPong() const { return heightMapPong; }
    bool IsPing() const { return ping; }

    void Update(float deltaTime);

    void DrawPlane();

    void SimulateMoss(float deltaTime);

private:
    // Used for switching between textures. The problem is we need to read and write to the height map. We need two
    // height maps so we can read from one then write to the other, and switch to read and write to the opposite next
    // compute shader dispatch
    bool ping = false;
    // Textures
    unsigned int heightMapPing; // Height map 1
    unsigned int heightMapPong; // Height map 2
    unsigned int heightMapPrev = 0;
    // Normals
    unsigned int normalMap;
    // Moss
    unsigned int mossPing = 0;
    unsigned int mossPong = 0;

    // Shaders
    unsigned int waterShader;
    // Compute shaders
    unsigned int simulateShader = 0;
    unsigned int rippleShader = 0;
    unsigned int normalShader = 0;
    unsigned int mossShader = 0;

    // Plane geometry
    unsigned int planeVAO = 0;
    unsigned int planeVBO = 0;
    unsigned int planeEBO = 0;
    unsigned int planeIndexCount = 0;

    void Init();

    void SwitchHeightMap();

    void AddRipple(glm::vec2 position, float strength);

    void GeneratePlane(int subdivisions);
};