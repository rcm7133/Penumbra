#include "model.h"
#include "../../../dependencies/stb_image.h"

#define CGLTF_IMPLEMENTATION
#include "../../../dependencies/cgltf.h"

Model::~Model()
{
    for (auto& sm : subMeshes) {
        glDeleteVertexArrays(1, &sm.VAO);
        glDeleteBuffers(1, &sm.VBO);
        glDeleteBuffers(1, &sm.EBO);
    }
    for (auto tex : loadedTextures)
        glDeleteTextures(1, &tex);
}

Material Model::DefaultMaterial()
{
    Material mat;
    mat.roughness = 0.5f;
    mat.metallic = 0.0f;
    mat.hasNormalMap = false;
    mat.hasHeightMap = false;
    return mat;
}

void Model::ApplyTexturePaths(int materialIndex,
                               const std::string& albedoPath,
                               const std::string& normalMapPath,
                               const std::string& heightMapPath,
                               const std::string& metallicRoughnessPath)
{
    if (materialIndex < 0 || materialIndex >= (int)materials.size()) return;
    Material& mat = materials[materialIndex];

    std::string directory = sourcePath.substr(0, sourcePath.find_last_of("/\\") + 1);

    if (!albedoPath.empty()) {
        mat.diffuseTexture = LoadTexture("", albedoPath);
        mat.texturePath = albedoPath;
    }

    if (!normalMapPath.empty()) {
        mat.normalMap = LoadTexture("", normalMapPath);
        mat.hasNormalMap = (mat.normalMap != 0);
        mat.normalMapPath = normalMapPath;
    } else {
        mat.hasNormalMap = false;
    }

    if (!heightMapPath.empty()) {
        mat.heightMap = LoadTexture("", heightMapPath);
        mat.hasHeightMap = (mat.heightMap != 0);
        mat.heightMapPath = heightMapPath;
    } else {
        mat.hasHeightMap = false;
    }

    if (!metallicRoughnessPath.empty()) {
        mat.metallicRoughnessMap = LoadTexture("", metallicRoughnessPath);
        mat.hasMetallicRoughnessMap = (mat.metallicRoughnessMap != 0);
        mat.metallicRoughnessMapPath = metallicRoughnessPath;
    } else {
        mat.hasMetallicRoughnessMap = false;
    }
}

bool Model::Load(const std::string& path) {
    sourcePath = path;

    cgltf_options options{};
    cgltf_data* data = nullptr;

    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
    if (result != cgltf_result_success) {
        std::cerr << "cgltf: failed to parse " << path << std::endl;
        return false;
    }

    result = cgltf_load_buffers(&options, data, path.c_str());
    if (result != cgltf_result_success) {
        std::cerr << "cgltf: failed to load buffers for " << path << std::endl;
        cgltf_free(data);
        return false;
    }

    // Extract directory for resolving relative texture paths
    std::string directory = path.substr(0, path.find_last_of("/\\") + 1);

    ProcessMaterials(data, directory);

    // Walk the scene graph and collect all mesh primitives
    for (cgltf_size i = 0; i < data->nodes_count; i++) {
        cgltf_node* node = &data->nodes[i];
        if (!node->mesh) continue;

        // Compute this node's world transform
        float matrix[16];
        cgltf_node_transform_world(node, matrix);
        glm::mat4 transform = glm::make_mat4(matrix);

        for (cgltf_size p = 0; p < node->mesh->primitives_count; p++) {
            ProcessPrimitive(&node->mesh->primitives[p], data, transform);
        }
    }

    std::cout << "Model loaded: " << path
              << " (" << subMeshes.size() << " sub-meshes, "
              << materials.size() << " materials)" << std::endl;

    cgltf_free(data);
    return true;
}

void Model::ProcessMaterials(const cgltf_data* data, const std::string& directory)
{
    for (cgltf_size i = 0; i < data->materials_count; i++) {
        const cgltf_material& gltfMat = data->materials[i];
        Material mat = DefaultMaterial();

        if (gltfMat.has_pbr_metallic_roughness) {
            const auto& pbr = gltfMat.pbr_metallic_roughness;

            mat.roughness = pbr.roughness_factor;
            mat.metallic  = pbr.metallic_factor;

            // Base color texture (albedo)
            if (pbr.base_color_texture.texture &&
                pbr.base_color_texture.texture->image)
            {
                cgltf_image* img = pbr.base_color_texture.texture->image;
                if (mat.hasMetallicRoughnessMap) {
                    mat.metallicRoughnessMapPath = img->uri ? img->uri : "(embedded)";
                }

                if (img->buffer_view) {
                    const uint8_t* bufData = (const uint8_t*)img->buffer_view->buffer->data
                                             + img->buffer_view->offset;
                    mat.diffuseTexture = LoadTextureFromMemory(bufData,
                                              (int)img->buffer_view->size);
                } else if (img->uri) {
                    mat.diffuseTexture = LoadTexture(directory, img->uri);
                }
                mat.texturePath = img->uri ? img->uri : "(embedded)";
            }

            // Metallic-roughness texture (G=roughness, B=metallic)
            if (pbr.metallic_roughness_texture.texture &&
                pbr.metallic_roughness_texture.texture->image)
            {
                cgltf_image* img = pbr.metallic_roughness_texture.texture->image;
                if (img->buffer_view) {
                    const uint8_t* bufData = (const uint8_t*)img->buffer_view->buffer->data
                                             + img->buffer_view->offset;
                    mat.metallicRoughnessMap = LoadTextureFromMemory(bufData,
                                                    (int)img->buffer_view->size);
                } else if (img->uri) {
                    mat.metallicRoughnessMap = LoadTexture(directory, img->uri);
                }
                mat.hasMetallicRoughnessMap = (mat.metallicRoughnessMap != 0);
                mat.metallicRoughnessMapPath = img->uri ? img->uri : "(embedded)";
            }
        }

        // Normal map (outside PBR block — glTF allows normals without PBR)
        if (gltfMat.normal_texture.texture &&
            gltfMat.normal_texture.texture->image)
        {
            cgltf_image* img = gltfMat.normal_texture.texture->image;
            if (img->buffer_view) {
                const uint8_t* bufData = (const uint8_t*)img->buffer_view->buffer->data
                                         + img->buffer_view->offset;
                mat.normalMap = LoadTextureFromMemory(bufData, (int)img->buffer_view->size);
            } else if (img->uri) {
                mat.normalMap = LoadTexture(directory, img->uri);
            }
            mat.hasNormalMap = (mat.normalMap != 0);
            mat.normalMapPath = img->uri ? img->uri : "(embedded)";
        }

        materials.push_back(mat);
    }
}

static const float* GetAccessorData(const cgltf_accessor* accessor)
{
    const cgltf_buffer_view* view = accessor->buffer_view;
    return (const float*)((const uint8_t*)view->buffer->data
                          + view->offset + accessor->offset);
}

void Model::ProcessPrimitive(const cgltf_primitive* prim,
                             const cgltf_data* data,
                             const glm::mat4& transform)
{
    if (prim->type != cgltf_primitive_type_triangles) return;

    // Find attribute accessors
    const cgltf_accessor* posAccessor    = nullptr;
    const cgltf_accessor* normalAccessor = nullptr;
    const cgltf_accessor* uvAccessor     = nullptr;
    const cgltf_accessor* tangentAccessor = nullptr;

    for (cgltf_size a = 0; a < prim->attributes_count; a++) {
        const cgltf_attribute& attr = prim->attributes[a];
        switch (attr.type) {
            case cgltf_attribute_type_position: posAccessor    = attr.data; break;
            case cgltf_attribute_type_normal:   normalAccessor = attr.data; break;
            case cgltf_attribute_type_texcoord: uvAccessor     = attr.data; break;
            case cgltf_attribute_type_tangent:  tangentAccessor = attr.data; break;
            default: break;
        }
    }

    if (!posAccessor) return;

    cgltf_size vertCount = posAccessor->count;
    glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(transform)));

    // Build interleaved vertex buffer: pos(3) + uv(2) + normal(3) + tangent(3) = 11 floats
    std::vector<float> vertices;
    vertices.reserve(vertCount * 11);

    for (cgltf_size v = 0; v < vertCount; v++) {
        // Position
        float pos[3] = {0, 0, 0};
        cgltf_accessor_read_float(posAccessor, v, pos, 3);
        glm::vec4 worldPos = transform * glm::vec4(pos[0], pos[1], pos[2], 1.0f);
        vertices.push_back(worldPos.x);
        vertices.push_back(worldPos.y);
        vertices.push_back(worldPos.z);

        // UV
        float uv[2] = {0, 0};
        if (uvAccessor) cgltf_accessor_read_float(uvAccessor, v, uv, 2);
        vertices.push_back(uv[0]);
        vertices.push_back(uv[1]);

        // Normal
        float norm[3] = {0, 1, 0};
        if (normalAccessor) cgltf_accessor_read_float(normalAccessor, v, norm, 3);
        glm::vec3 worldNormal = glm::normalize(normalMat * glm::vec3(norm[0], norm[1], norm[2]));
        vertices.push_back(worldNormal.x);
        vertices.push_back(worldNormal.y);
        vertices.push_back(worldNormal.z);

        // Tangent (glTF tangents are vec4 with w = handedness)
        float tan[4] = {1, 0, 0, 1};
        if (tangentAccessor) cgltf_accessor_read_float(tangentAccessor, v, tan, 4);
        glm::vec3 worldTangent = glm::normalize(normalMat * glm::vec3(tan[0], tan[1], tan[2]));
        // Re-orthogonalize
        worldTangent = glm::normalize(worldTangent - worldNormal * glm::dot(worldNormal, worldTangent));
        vertices.push_back(worldTangent.x);
        vertices.push_back(worldTangent.y);
        vertices.push_back(worldTangent.z);
    }

    // Compute tangents if not provided
    // (needed for normal mapping / parallax)
    std::vector<unsigned int> indices;

    if (prim->indices) {
        indices.resize(prim->indices->count);
        for (cgltf_size i = 0; i < prim->indices->count; i++) {
            indices[i] = (unsigned int)cgltf_accessor_read_index(prim->indices, i);
        }
    } else {
        indices.resize(vertCount);
        for (cgltf_size i = 0; i < vertCount; i++)
            indices[i] = (unsigned int)i;
    }

    // If no tangent attribute, compute tangents from UV deltas
    if (!tangentAccessor && uvAccessor) {
        std::vector<glm::vec3> tangents(vertCount, glm::vec3(0));
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            unsigned int i0 = indices[i], i1 = indices[i+1], i2 = indices[i+2];

            glm::vec3 p0(vertices[i0*11], vertices[i0*11+1], vertices[i0*11+2]);
            glm::vec3 p1(vertices[i1*11], vertices[i1*11+1], vertices[i1*11+2]);
            glm::vec3 p2(vertices[i2*11], vertices[i2*11+1], vertices[i2*11+2]);

            glm::vec2 uv0(vertices[i0*11+3], vertices[i0*11+4]);
            glm::vec2 uv1(vertices[i1*11+3], vertices[i1*11+4]);
            glm::vec2 uv2(vertices[i2*11+3], vertices[i2*11+4]);

            glm::vec3 edge1 = p1 - p0, edge2 = p2 - p0;
            glm::vec2 dUV1 = uv1 - uv0, dUV2 = uv2 - uv0;

            float denom = dUV1.x * dUV2.y - dUV2.x * dUV1.y;
            float f = (std::abs(denom) > 1e-6f) ? (1.0f / denom) : 0.0f;

            glm::vec3 tangent;
            tangent.x = f * (dUV2.y * edge1.x - dUV1.y * edge2.x);
            tangent.y = f * (dUV2.y * edge1.y - dUV1.y * edge2.y);
            tangent.z = f * (dUV2.y * edge1.z - dUV1.y * edge2.z);

            tangents[i0] += tangent;
            tangents[i1] += tangent;
            tangents[i2] += tangent;
        }

        for (cgltf_size v = 0; v < vertCount; v++) {
            glm::vec3 n(vertices[v*11+5], vertices[v*11+6], vertices[v*11+7]);
            glm::vec3 t = glm::normalize(tangents[v]);
            t = glm::normalize(t - n * glm::dot(n, t));
            vertices[v*11+8]  = t.x;
            vertices[v*11+9]  = t.y;
            vertices[v*11+10] = t.z;
        }
    }

    // Upload to GPU
    SubMesh sm;

    glGenVertexArrays(1, &sm.VAO);
    glGenBuffers(1, &sm.VBO);
    glGenBuffers(1, &sm.EBO);

    glBindVertexArray(sm.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, sm.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sm.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 indices.data(), GL_STATIC_DRAW);

    int stride = 11 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);

    sm.indexCount = (int)indices.size();

    // Map material
    if (prim->material) {
        cgltf_size matIdx = prim->material - data->materials;
        sm.materialIndex = (int)matIdx;
    }

    subMeshes.push_back(sm);
}

void Model::Draw(unsigned int shader, int roughnessLoc, int metallicLoc,
                 int hasNormalMapLoc, int hasHeightMapLoc, int heightScaleLoc,
                 int emissiveColorLoc, int emissiveIntensityLoc)
{
    int hasMetRoughMapLoc = glGetUniformLocation(shader, "hasMetallicRoughnessMap");
    static Material defaultMat = DefaultMaterial();

    for (auto& sm : subMeshes) {
        Material& mat = (sm.materialIndex >= 0 && sm.materialIndex < (int)materials.size())
                        ? materials[sm.materialIndex]
                        : defaultMat;

        glUniform1f(roughnessLoc, mat.roughness);
        glUniform1f(metallicLoc,  mat.metallic);
        glUniform3fv(emissiveColorLoc, 1, glm::value_ptr(mat.emissiveColor));
        glUniform1f(emissiveIntensityLoc, mat.emissiveIntensity);
        glUniform1i(hasNormalMapLoc, mat.hasNormalMap ? 1 : 0);
        glUniform1i(hasHeightMapLoc, mat.hasHeightMap ? 1 : 0);
        glUniform1f(heightScaleLoc,  mat.heightScale);
        glUniform1i(hasMetRoughMapLoc, mat.hasMetallicRoughnessMap ? 1 : 0);
        glUniform3fv(emissiveColorLoc, 1, &mat.emissiveColor.x);
        glUniform1f(emissiveIntensityLoc, mat.emissiveIntensity);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mat.diffuseTexture);

        if (mat.hasNormalMap) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, mat.normalMap);
        }

        if (mat.hasHeightMap) {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, mat.heightMap);
        }

        if (mat.hasMetallicRoughnessMap) {
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, mat.metallicRoughnessMap);
        }

        glBindVertexArray(sm.VAO);
        glDrawElements(GL_TRIANGLES, sm.indexCount, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}

void Model::DrawGeometry()
{
    for (auto& sm : subMeshes) {
        glBindVertexArray(sm.VAO);
        glDrawElements(GL_TRIANGLES, sm.indexCount, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}

unsigned int Model::UploadTexture(unsigned char* pixels, int width, int height, int channels)
{
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum format = GL_RGB;
    if (channels == 1) format = GL_RED;
    else if (channels == 3) format = GL_RGB;
    else if (channels == 4) format = GL_RGBA;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
                 GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    loadedTextures.push_back(tex);
    return tex;
}

unsigned int Model::LoadTexture(const std::string& directory, const std::string& uri)
{
    std::string fullPath = directory + uri;

    // Check cache
    auto it = textureCache.find(fullPath);
    if (it != textureCache.end()) return it->second;

    int w, h, channels;
    stbi_set_flip_vertically_on_load(false); // glTF textures are top-down
    unsigned char* data = stbi_load(fullPath.c_str(), &w, &h, &channels, 0);
    if (!data) {
        std::cerr << "Model: failed to load texture: " << fullPath << std::endl;
        return 0;
    }

    unsigned int tex = UploadTexture(data, w, h, channels);
    stbi_image_free(data);

    textureCache[fullPath] = tex;
    return tex;
}

unsigned int Model::LoadTextureFromMemory(const unsigned char* data, int length)
{
    int w, h, channels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* pixels = stbi_load_from_memory(data, length, &w, &h, &channels, 0);
    if (!pixels) {
        std::cerr << "Model: failed to load embedded texture" << std::endl;
        return 0;
    }

    unsigned int tex = UploadTexture(pixels, w, h, channels);
    stbi_image_free(pixels);
    return tex;
}

