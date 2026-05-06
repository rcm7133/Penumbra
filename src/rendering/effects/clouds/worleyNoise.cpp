#include "worleyNoise.h"

#include "rendering/shaderutils.h"

WorleyNoiseGenerator::WorleyNoiseGenerator() {
    // Load compute shader
    texturePopulateComputeShader = ShaderUtils::LoadComputeShader("../shaders/compute/cloud/texturePopulateCompute.glsl");
}

void WorleyNoiseGenerator::SetResolution(int newTexRes, int r, int g, int b, int a) {
    resolution = newTexRes;
    voxelResolutionR = r;
    voxelResolutionG = g;
    voxelResolutionB = b;
    voxelResolutionA = a;

    pointDataR.resize(voxelResolutionR * voxelResolutionR * voxelResolutionR);
    pointDataG.resize(voxelResolutionG * voxelResolutionG * voxelResolutionG);
    pointDataB.resize(voxelResolutionB * voxelResolutionB * voxelResolutionB);
    pointDataA.resize(voxelResolutionA * voxelResolutionA * voxelResolutionA);

    CreateBuffers();
}

void WorleyNoiseGenerator::CreateBuffers() {
    if (pointRSSBO) glDeleteBuffers(1, &pointRSSBO);
    if (pointGSSBO) glDeleteBuffers(1, &pointGSSBO);
    if (pointBSSBO) glDeleteBuffers(1, &pointBSSBO);
    if (pointASSBO) glDeleteBuffers(1, &pointASSBO);

    // R Channel SSBO index 0
    glGenBuffers(1, &pointRSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, pointRSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, pointDataR.size() * sizeof(glm::vec4), pointDataR.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pointRSSBO);
    // G Channel SSBO index 1
    glGenBuffers(1, &pointGSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, pointGSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, pointDataG.size() * sizeof(glm::vec4), pointDataG.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, pointGSSBO);
    // B Channel SSBO index 2
    glGenBuffers(1, &pointBSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, pointBSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, pointDataB.size() * sizeof(glm::vec4), pointDataB.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, pointBSSBO);
    // A Channel SSBO index 3
    glGenBuffers(1, &pointASSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, pointASSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, pointDataA.size() * sizeof(glm::vec4), pointDataA.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, pointASSBO);

    // Unbind
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

GLuint WorleyNoiseGenerator::GenerateNoise() {
    // Setup buffers and vectors
    pointDataR.resize(voxelResolutionR * voxelResolutionR * voxelResolutionR);
    pointDataG.resize(voxelResolutionG * voxelResolutionG * voxelResolutionG);
    pointDataB.resize(voxelResolutionB * voxelResolutionB * voxelResolutionB);
    pointDataA.resize(voxelResolutionA * voxelResolutionA * voxelResolutionA);

    PopulateRandomPoints(pointDataR, voxelResolutionR);
    PopulateRandomPoints(pointDataG, voxelResolutionG);
    PopulateRandomPoints(pointDataB, voxelResolutionB);
    PopulateRandomPoints(pointDataA, voxelResolutionA);
    CreateBuffers();

    GLuint texture;
    // Make 3d Texture
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_3D, texture);
    // Specify wrapping and filtering
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Populate 3d texture from data
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, resolution, resolution, resolution,
        0, GL_RGBA, GL_FLOAT, nullptr);
    // Send image into compute shader
    glBindImageTexture(4, texture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    // Dispatch Compute shader
    glUseProgram(texturePopulateComputeShader);
    glUniform1i(glGetUniformLocation(texturePopulateComputeShader, "voxelResolutionR"), voxelResolutionR);
    glUniform1i(glGetUniformLocation(texturePopulateComputeShader, "voxelResolutionG"), voxelResolutionG);
    glUniform1i(glGetUniformLocation(texturePopulateComputeShader, "voxelResolutionB"), voxelResolutionB);
    glUniform1i(glGetUniformLocation(texturePopulateComputeShader, "voxelResolutionA"), voxelResolutionA);
    glUniform1i(glGetUniformLocation(texturePopulateComputeShader, "inverted"), inverted);
    int groupSize = 8;
    glDispatchCompute(
        (resolution + groupSize - 1) / groupSize,
        (resolution + groupSize - 1) / groupSize,
        (resolution + groupSize - 1) / groupSize
    );
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    return texture;
}

int WorleyNoiseGenerator::XYZToIndex(const glm::vec3& point, const int voxelResolution) {
    return point.x + point.y * voxelResolution + point.z * voxelResolution * voxelResolution;
}

glm::vec3 WorleyNoiseGenerator::VoxelFloor(const glm::vec3& point, const int voxelResolution) {
    glm::vec3 voxelSize = (max - min) / (float)voxelResolution;
    return min + glm::vec3(point) * voxelSize;
}

glm::vec3 WorleyNoiseGenerator::VoxelCeil(const glm::vec3& point, const int voxelResolution) {
    glm::vec3 voxelSize = (max - min) / (float)voxelResolution;
    return min + (glm::vec3(point) + glm::vec3(1.0f)) * voxelSize;
}

void WorleyNoiseGenerator::PopulateRandomPoints(std::vector<glm::vec4>& points, const int voxelResolution) {
    std::random_device rd;
    std::mt19937 gen(rd());

    for (int x = 0; x < voxelResolution; x++) {
        for (int y = 0; y < voxelResolution; y++) {
            for (int z = 0; z < voxelResolution; z++) {
                glm::vec3 voxelCoord = glm::vec3(x, y, z);
                glm::vec3 min = VoxelFloor(voxelCoord, voxelResolution);
                glm::vec3 max = VoxelCeil(voxelCoord, voxelResolution);

                std::uniform_real_distribution<float> randX(min.x, max.x);
                std::uniform_real_distribution<float> randY(min.y, max.y);
                std::uniform_real_distribution<float> randZ(min.z, max.z);

                points[XYZToIndex(glm::vec3(x, y, z), voxelResolution)] = glm::vec4(randX(gen), randY(gen), randZ(gen), 1.0);
            }
        }
    }
}