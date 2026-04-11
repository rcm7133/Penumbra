#pragma once
#include "material.h"

class Mesh
{
public:
    Material material;

    Mesh(const std::string& filePath, const std::string& texturePath,
         const std::string& normalMapPath = "",
         const std::string& heightMapPath = "");
    ~Mesh();
    void Draw();
    int vertexCount;
private:
    unsigned int VAO, VBO, EBO;
    unsigned int textureID;
    void Setup(const std::vector<float>& vertices, const std::vector<unsigned int>& indices);
    void LoadTexture(const std::string& path);
    void LoadNormalMap(const std::string& path);
    void LoadHeightMap(const std::string& path);
};