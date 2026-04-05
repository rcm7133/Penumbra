#pragma once
#include "config.h"

extern float CAMERA_SPEED;

class Camera
{
public:
    Transform transform;
    float sensitivity = 0.001f;
    float yaw   = 0.0f;
    float pitch = 0.0f;

    Camera(glm::vec3 startPos = glm::vec3(0, 1, 3)) {
        transform.position = startPos;
    }

    glm::mat4 GetViewMatrix() const {
        return glm::lookAt(
            transform.position,
            transform.position + transform.Forward(),
            glm::vec3(0, 1, 0)
        );
    }

    void ProcessKeyboard(GLFWwindow* window, float deltaTime) {
        float velocity = CAMERA_SPEED * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            transform.position += transform.Forward() * velocity;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            transform.position -= transform.Forward() * velocity;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            transform.position -= transform.Right() * velocity;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            transform.position += transform.Right() * velocity;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            transform.position.y += velocity;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            transform.position.y -= velocity;
    }

    void ProcessMouse(float xOffset, float yOffset) {
        yaw   += xOffset * sensitivity;
        pitch += yOffset * sensitivity;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);

        transform.rotation = glm::quat(glm::radians(glm::vec3(pitch, -yaw, 0.0f)));
    }
};