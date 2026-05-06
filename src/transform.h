#pragma once
#include "config.h"

struct Transform
{
    glm::vec3 position = glm::vec3(0, 0, 0);
    glm::quat rotation = glm::quat(1, 0, 0, 0); // identity
    glm::vec3 scale = glm::vec3(1, 1, 1);

    glm::vec3 Forward() const {
        return rotation * glm::vec3(0, 0, -1);
    }

    glm::vec3 Right() const {
        return rotation * glm::vec3(1, 0, 0);
    }

    glm::vec3 Up() const {
        return rotation * glm::vec3(0, 1, 0);
    }

    glm::vec3 GetEulerDegrees() const {
        return glm::degrees(glm::eulerAngles(rotation));
    }

    void SetEulerDegrees(const glm::vec3& degrees) {
        rotation = glm::quat(glm::radians(degrees));
    }

    glm::mat4 GetMatrix() const {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
        m *= glm::mat4_cast(rotation);
        m = glm::scale(m, scale);
        return m;
    }
};