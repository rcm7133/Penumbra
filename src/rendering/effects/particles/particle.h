#pragma once
#include "../../../config.h"

struct Particle
{
    glm::vec3 position;
    float lifetime;
    glm::vec3 velocity;
    float maxLifetime;
    glm::vec4 color;
    float size;
    float padding[3];
};
