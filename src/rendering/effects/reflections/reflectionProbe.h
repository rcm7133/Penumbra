#pragma once
#include "../../../config.h"

struct ReflectionProbe {
    glm::vec3 boundsMin;
    glm::vec3 boundsMax;
    unsigned int cubemap = 0;
    float blendRadius = 1.0f;
    int resolution = 128;
    bool baked = false;
};