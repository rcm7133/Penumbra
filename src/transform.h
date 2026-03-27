#pragma once
#include "config.h"

struct Transform
{
    glm::vec3 position = glm::vec3(0, 0, 0);
    glm::vec3 rotation = glm::vec3(0, 0, 0); // Euler angles in degrees
    glm::vec3 scale = glm::vec3(1, 1, 1);

    glm::vec3 Forward() const {
        glm::vec3 direction;
        direction.x = cos(glm::radians(rotation.y)) * cos(glm::radians(rotation.x));
        direction.y = sin(glm::radians(rotation.x));
        direction.z = sin(glm::radians(rotation.y)) * cos(glm::radians(rotation.x));
        return glm::normalize(direction);
    }

    glm::vec3 Right() const {
        return glm::normalize(glm::cross(Forward(), glm::vec3(0, 1, 0)));
    }

    glm::vec3 Up() const {
        return glm::normalize(glm::cross(Right(), Forward()));
    }

    glm::mat4 GetMatrix() const
    {
        glm::mat4 matrix = glm::mat4(1.0f);
        matrix = glm::translate(matrix, position);
        matrix = glm::rotate(matrix, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        matrix = glm::rotate(matrix, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        matrix = glm::rotate(matrix, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        matrix = glm::scale(matrix, scale);
        return matrix;
    }
};