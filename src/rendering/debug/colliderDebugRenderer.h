#pragma once
#include "config.h"

class ColliderDebugRenderer {
public:
    void Init();
    void AddLine(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color);
    void AddBox(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& halfExtent, const glm::vec3& color);
    void AddSphere(const glm::vec3& pos, float radius, const glm::vec3& color, int segments = 16);
    void AddCapsule(const glm::vec3& pos, const glm::quat& rot, float halfHeight, float radius, const glm::vec3& color, int segments = 16);
    void Render(const glm::mat4& view, const glm::mat4& projection);
private:
    struct Vertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    std::vector<Vertex> vertices;
    unsigned int VAO = 0, VBO = 0;
    unsigned int shader = 0;
};