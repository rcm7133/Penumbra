#pragma once

#include "config.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

enum class BodyMotion { Static, Dynamic, Kinematic };

struct RigidBody {
    JPH::BodyID bodyID;
    BodyMotion motion = BodyMotion::Static;

    enum ShapeType { Box, Sphere, Mesh } shapeType = Box;
    glm::vec3 halfExtent = glm::vec3(0.5f);  // for box
    float radius = 0.5f;                      // for sphere
    float mass = 1.0f;
    float friction = 0.5f;
    float restitution = 0.3f;
};