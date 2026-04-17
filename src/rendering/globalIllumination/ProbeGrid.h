#pragma once
#include "../../config.h"

struct LightProbe
{
    glm::vec3 position;
    glm::vec3 shCoeffs[9]; // 9 SH coefficients each RGB
    bool baked = false;
};

class ProbeGrid {
public:
    // Grid bounds
    glm::vec3 boundsMin = glm::vec3(-10, -2, -10);
    glm::vec3 boundsMax = glm::vec3(10, 6, 10);

    // Probe count per axis
    int countX = 6;
    int countY = 3;
    int countZ = 6;

    std::vector<LightProbe> probes;

    // Build probe grid from current bounds
    void Init() {
        probes.clear();
        probes.resize(countX * countY * countZ);

        for (int z = 0; z < countZ; z++) {
            for (int y = 0; y < countY; y++) {
                for (int x = 0; x < countX; x++) {
                    // Position in buffer 3D->1D
                    int idx = z * countY * countX + y * countX + x;
                    // Calculate the positon of the probe in the grid
                    float px = (countX > 1) ? (float)x / (countX - 1) : 0.5f;
                    float py = (countY > 1) ? (float)y / (countY - 1) : 0.5f;
                    float pz = (countZ > 1) ? (float)z / (countZ - 1) : 0.5f;

                    probes[idx].position = glm::mix(boundsMin, boundsMax, glm::vec3(px, py, pz));
                    probes[idx].baked = false;
                }
            }
        }
    }

    void Load()
    {
        for (int z = 0; z < countZ; z++) {
            for (int y = 0; y < countY; y++) {
                for (int x = 0; x < countX; x++) {
                    // Position in buffer 3D->1D
                    int idx = z * countY * countX + y * countX + x;
                    // Calculate the positon of the probe in the grid
                    float px = (countX > 1) ? (float)x / (countX - 1) : 0.5f;
                    float py = (countY > 1) ? (float)y / (countY - 1) : 0.5f;
                    float pz = (countZ > 1) ? (float)z / (countZ - 1) : 0.5f;

                    probes[idx].position = glm::mix(boundsMin, boundsMax, glm::vec3(px, py, pz));
                }
            }
        }
    }

    // Given a world positon find the 8 surrounding probes and interpolate weights for a world position
    // Returns false if the point is outside the probe grid
    bool Sample(const glm::vec3& worldPos, glm::vec3 outSH[9]) const {
        // Normalize the grid space and clamp to bounds
        glm::vec3 gridPos = (worldPos - boundsMin) / (boundsMax - boundsMin);
        gridPos = glm::clamp(gridPos, glm::vec3(0), glm::vec3(1));

        // Convert to grid indcies
        float fx = gridPos.x * (countX - 1);
        float fy = gridPos.y * (countY - 1);
        float fz = gridPos.z * (countZ - 1);

        // Get integer indices
        int x0 = glm::clamp((int)floor(fx), 0, countX - 2);
        int y0 = glm::clamp((int)floor(fy), 0, countY - 2);
        int z0 = glm::clamp((int)floor(fz), 0, countZ - 2);

        // Fractional part for interpolation
        float dx = fx - x0;
        float dy = fy - y0;
        float dz = fz - z0;

        // Trilinear interpolation of SH coefficients
        for (int i = 0; i < 9; i++) {
            outSH[i] = glm::vec3(0);
        }

        for (int cz = 0; cz <= 1; cz++) {
            for (int cy = 0; cy <= 1; cy++) {
                for (int cx = 0; cx <= 1; cx++) {
                    int idx = (z0 + cz) * countY * countX + (y0 + cy) * countX + (x0 + cx);

                    float weight = (cx ? dx : 1 - dx) * (cy ? dy : 1 - dy) * (cz ? dz : 1 - dz);

                    const auto& probe = probes[idx];

                    if (!probe.baked) continue;

                    for (int i = 0; i < 9; i++) {
                        outSH[i] += probe.shCoeffs[i] * weight;
                    }
                }
            }
        }
        return true;
    }

    // Get probe index from grid coordinates
    int GetIndex(int x, int y, int z) const {
        return z * countY * countX + y * countX + x;
    }

    int TotalProbes() const { return countX * countY * countZ; }

};