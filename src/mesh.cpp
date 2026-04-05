#include "config.h"
#include "mesh.h"
#include "stb_image.h"

Mesh::Mesh(const std::string& filePath, const std::string& texturePath, float shininess, const std::string& normalMapPath)
{
    material.modelPath = filePath;
    material.texturePath = texturePath;
    material.normalMapPath = normalMapPath;


    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::vec3> normals;

    // Each "expanded" vertex will hold pos, uv, normal (before tangent calc)
    std::vector<glm::vec3> outPositions;
    std::vector<glm::vec2> outUVs;
    std::vector<glm::vec3> outNormals;
    std::vector<unsigned int> indices;

    std::ifstream file(filePath);
    std::string line;

    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::istringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "v")
        {
            glm::vec3 pos;
            ss >> pos.x >> pos.y >> pos.z;
            positions.push_back(pos);
        }
        else if (token == "vt")
        {
            glm::vec2 uv;
            ss >> uv.x >> uv.y;
            texCoords.push_back(uv);
        }
        else if (token == "vn")
        {
            glm::vec3 normal;
            ss >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        }
        else if (token == "f")
        {
            std::string faceToken;
            while (ss >> faceToken)
            {
                std::replace(faceToken.begin(), faceToken.end(), '/', ' ');
                std::istringstream fss(faceToken);
                unsigned int pi, ti, ni;
                fss >> pi >> ti >> ni;

                if (pi == 0 || ti == 0 || ni == 0) continue;
                if (pi > positions.size() || ti > texCoords.size() || ni > normals.size()) continue;

                outPositions.push_back(positions[pi - 1]);
                outUVs.push_back(texCoords[ti - 1]);
                outNormals.push_back(normals[ni - 1]);

                indices.push_back(indices.size());
            }
        }
    }

    std::vector<glm::vec3> tangents(outPositions.size(), glm::vec3(0.0f));

    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        unsigned int i0 = indices[i];
        unsigned int i1 = indices[i + 1];
        unsigned int i2 = indices[i + 2];

        glm::vec3 edge1 = outPositions[i1] - outPositions[i0];
        glm::vec3 edge2 = outPositions[i2] - outPositions[i0];

        glm::vec2 dUV1 = outUVs[i1] - outUVs[i0];
        glm::vec2 dUV2 = outUVs[i2] - outUVs[i0];

        float denom = (dUV1.x * dUV2.y - dUV2.x * dUV1.y);
        float f = (std::abs(denom) > 1e-6f) ? (1.0f / denom) : 0.0f;

        glm::vec3 tangent;
        tangent.x = f * (dUV2.y * edge1.x - dUV1.y * edge2.x);
        tangent.y = f * (dUV2.y * edge1.y - dUV1.y * edge2.y);
        tangent.z = f * (dUV2.y * edge1.z - dUV1.y * edge2.z);

        tangents[i0] += tangent;
        tangents[i1] += tangent;
        tangents[i2] += tangent;
    }


    std::vector<float> vertices;
    vertices.reserve(outPositions.size() * 11);

    for (size_t i = 0; i < outPositions.size(); i++)
    {
        glm::vec3 t = glm::normalize(tangents[i]);

        // Gram-Schmidt orthogonalize tangent against normal
        glm::vec3 n = glm::normalize(outNormals[i]);
        t = glm::normalize(t - n * glm::dot(n, t));

        vertices.push_back(outPositions[i].x);
        vertices.push_back(outPositions[i].y);
        vertices.push_back(outPositions[i].z);
        vertices.push_back(outUVs[i].x);
        vertices.push_back(outUVs[i].y);
        vertices.push_back(outNormals[i].x);
        vertices.push_back(outNormals[i].y);
        vertices.push_back(outNormals[i].z);
        vertices.push_back(t.x);
        vertices.push_back(t.y);
        vertices.push_back(t.z);
    }

    vertexCount = indices.size();
    Setup(vertices, indices);
    LoadTexture(texturePath);

    if (!normalMapPath.empty())
        LoadNormalMap(normalMapPath);

    SetShininess(shininess);
}

Mesh::~Mesh()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void Mesh::Draw()
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, vertexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Mesh::Setup(const std::vector<float>& vertices, const std::vector<unsigned int>& indices)
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    int stride = 11 * sizeof(float);

    // Position (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    // UV (location = 1)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Normal (location = 2)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // Tangent (location = 3)
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);
}

void Mesh::LoadTexture(const std::string& path)
{
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Wrapping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, channels;
    stbi_set_flip_vertically_on_load(true); // OBJ UVs usually expect this
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);

    if (data)
    {
        GLenum format = GL_RGB;
        if (channels == 1) format = GL_RED;
        else if (channels == 3) format = GL_RGB;
        else if (channels == 4) format = GL_RGBA;

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        std::cerr << "Failed to load texture: " << path << std::endl;
    }

    stbi_image_free(data);
}

void Mesh::LoadNormalMap(const std::string& path)
{
    glGenTextures(1, &material.normalMap);
    glBindTexture(GL_TEXTURE_2D, material.normalMap);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);

    if (data)
    {
        GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        material.hasNormalMap = true;
    }
    else
    {
        std::cerr << "Failed to load normal map: " << path << std::endl;
        material.hasNormalMap = false;
    }

    stbi_image_free(data);
}