#pragma once
#include "config.h"

struct FogVolume {
    glm::vec3 boundsMin = glm::vec3(-5.0f, 0.0f, -2.0f);
    glm::vec3 boundsMax = glm::vec3(5.0f, 3.0f, 2.0f);
    float density = 0.5f;
    float scale = 0.5f;
    float scrollSpeed = 0.1f;
};