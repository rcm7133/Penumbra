#pragma once
#include "material.h"

class Mesh
{
public:
    Material material;

    Mesh(const std::string& filePath, const std::string& texturePath, float shininess, const std::string& normalMapPath);
    ~Mesh();
    void Draw();
    int vertexCount;
    [[nodiscard]] float GetShininess() const { return m_shininess; }
    void SetShininess(const float value) { m_shininess = value; }
private:
    unsigned int VAO, VBO, EBO;
    unsigned int textureID;
    void Setup(const std::vector<float>& vertices, const std::vector<unsigned int>& indices);
    void LoadTexture(const std::string& path);
    void LoadNormalMap(const std::string& path);
    float m_shininess;
};