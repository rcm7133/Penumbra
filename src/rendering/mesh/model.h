#pragma once
#include "../../config.h"
#include "material.h"

struct SubMesh
{
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int EBO = 0;
    int indexCount = 0;
    int materialIndex = -1; // index into Model::materials, -1 = default
    glm::mat4 nodeTransform = glm::mat4(1.0f);

    std::vector<glm::vec3> positions;
    std::vector<unsigned int> indices;
    std::vector<glm::vec2> uvs;
};

class Model {
public:
    std::vector<SubMesh> subMeshes;
    std::vector<Material> materials;
    std::string sourcePath;

    Model() = default;
    ~Model();

    bool Load(const std::string& path);

    void Draw(unsigned int shader, int roughnessLoc, int metallicLoc,
                 int hasNormalMapLoc, int hasHeightMapLoc, int heightScaleLoc,
                 int emissiveColorLoc, int emissiveIntensityLoc);

    void DrawGeometry();

    // For objects without material
    static Material DefaultMaterial();

    unsigned int LoadTexture(const std::string& directory, const std::string& uri);

    void ApplyTexturePaths(int materialIndex,
                           const std::string& albedoPath,
                           const std::string& normalMapPath = "",
                           const std::string& heightMapPath = "",
                           const std::string& metallicRoughnessPath = "");

private:
    std::vector<unsigned int> loadedTextures;
    unsigned int LoadTextureFromMemory(const unsigned char* data, int length);
    unsigned int UploadTexture(unsigned char* pixels, int width, int height, int channels);

    void ProcessPrimitive(const struct cgltf_primitive* prim,
                          const struct cgltf_data* data,
                          const glm::mat4& transform);

    void ProcessMaterials(const struct cgltf_data* data,
                          const std::string& directory);

    // URI to GL handle to avoid loading a texture twice
    std::unordered_map<std::string, unsigned int> textureCache;
};