#include "interactiveWater.h"

#include "rendering/shaderutils.h"

void InteractiveWater::Init() {
    // Initialize Textures
    // Height maps
    glGenTextures(1, &heightMapPing);
    glBindTexture(GL_TEXTURE_2D, heightMapPing);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, resolution, resolution,
        0, GL_RED, GL_FLOAT, nullptr);

    glGenTextures(1, &heightMapPong);
    glBindTexture(GL_TEXTURE_2D, heightMapPong);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, resolution, resolution,
        0, GL_RED, GL_FLOAT, nullptr);

    glGenTextures(1, &heightMapPrev);
    glBindTexture(GL_TEXTURE_2D, heightMapPrev);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, resolution, resolution,
        0, GL_RED, GL_FLOAT, nullptr);

    // Normal map
    glGenTextures(1, &normalMap);
    glBindTexture(GL_TEXTURE_2D, normalMap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, resolution, resolution,
        0, GL_RGB, GL_FLOAT, nullptr);


    // Moss textures
    glGenTextures(1, &mossPing);
    glBindTexture(GL_TEXTURE_2D, mossPing);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, resolution, resolution,
        0, GL_RED, GL_FLOAT, nullptr);

    glGenTextures(1, &mossPong);
    glBindTexture(GL_TEXTURE_2D, mossPong);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, resolution, resolution,
        0, GL_RED, GL_FLOAT, nullptr);

    std::vector<float> initialData(resolution * resolution, 0.0f);
    auto setCircle = [&](int cx, int cy, int radius, float value) {
        for (int y = -radius; y <= radius; y++)
            for (int x = -radius; x <= radius; x++)
                if (x*x + y*y <= radius*radius) {
                    int px = cx + x, py = cy + y;
                    if (px >= 0 && px < resolution && py >= 0 && py < resolution)
                        initialData[py * resolution + px] = value;
                }
    };
    setCircle(resolution / 2,     resolution / 2,     6, 1.0f);
    setCircle(resolution / 4,     resolution / 4,     4, 0.8f);
    setCircle(resolution * 3 / 4, resolution / 4,     4, 0.8f);
    setCircle(resolution / 4,     resolution * 3 / 4, 4, 0.8f);
    setCircle(resolution * 3 / 4, resolution * 3 / 4, 4, 0.8f);

    std::vector<float> mossData(resolution * resolution, 0.5f);

    glBindTexture(GL_TEXTURE_2D, heightMapPing);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, resolution, resolution, GL_RED, GL_FLOAT, initialData.data());
    glBindTexture(GL_TEXTURE_2D, heightMapPong);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, resolution, resolution, GL_RED, GL_FLOAT, initialData.data());
    glBindTexture(GL_TEXTURE_2D, heightMapPrev);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, resolution, resolution,GL_RED, GL_FLOAT, initialData.data());

    glBindTexture(GL_TEXTURE_2D, mossPing);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, resolution, resolution,GL_RED, GL_FLOAT, mossData.data());
    glBindTexture(GL_TEXTURE_2D, mossPong);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, resolution, resolution,GL_RED, GL_FLOAT, mossData.data());
    

    waterShader = ShaderUtils::MakeShaderProgram("../shaders/water/waterVert.glsl", "../shaders/water/waterFrag.glsl");

    simulateShader = ShaderUtils::LoadComputeShader("../shaders/compute/water/waterSimulateCompute.glsl");
    normalShader = ShaderUtils::LoadComputeShader("../shaders/compute/water/waterNormalCompute.glsl");
    rippleShader = ShaderUtils::LoadComputeShader("../shaders/compute/water/waterRippleCompute.glsl");
    mossShader = ShaderUtils::LoadComputeShader("../shaders/compute/water/mossDensityCompute.glsl");

    GeneratePlane(128);
}

unsigned int InteractiveWater::GetHeightMap() const {
    return heightMapPing; // after rotation, ping is always the latest output
}

void InteractiveWater::ComputeHeightMap() {
    glUseProgram(simulateShader);

    glUniform1f(glGetUniformLocation(simulateShader, "waveSpeed"), waveSpeed);
    glUniform1f(glGetUniformLocation(simulateShader, "dampening"), dampening);

    // current = ping, previous = prev, output = pong
    glBindImageTexture(0, heightMapPing, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_R16F); // current
    glBindImageTexture(1, heightMapPrev, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_R16F); // previous
    glBindImageTexture(2, heightMapPong, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R16F); // output

    glDispatchCompute(resolution / 16, resolution / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    // Rotate prev = ping, ping = pong, pong = prev
    unsigned int temp = heightMapPrev;
    heightMapPrev = heightMapPing;
    heightMapPing = heightMapPong;
    heightMapPong = temp;
}

void InteractiveWater::ComputeNormals() {
    glUseProgram(normalShader);

    glUniform1f(glGetUniformLocation(normalShader, "heightScale"), rippleStrength);
    glUniform1f(glGetUniformLocation(normalShader, "texelSize"), 1.0f / resolution);

    glBindImageTexture(0, GetHeightMap(), 0, GL_FALSE, 0, GL_READ_ONLY,  GL_R16F);
    glBindImageTexture(1, normalMap,      0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    glDispatchCompute(resolution / 16, resolution / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void InteractiveWater::AddRipple(glm::vec2 uv, float strength) {
    int x = (int)(uv.x * resolution);
    int y = (int)(uv.y * resolution);

    glBindImageTexture(0, heightMapPing, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R16F);
    glUseProgram(rippleShader);
    glUniform2i(glGetUniformLocation(rippleShader, "ripplePos"), x, y);
    glUniform1f(glGetUniformLocation(rippleShader, "strength"), strength);
    int scaledRadius = std::max(3, resolution / 64);
    glUniform1i(glGetUniformLocation(rippleShader, "radius"), scaledRadius);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void InteractiveWater::AddRippleWorld(glm::vec3 worldPos, glm::vec3 waterWorldPos, float waterSize) {
    // Convert world xz to UV space
    glm::vec2 uv;
    uv.x = (worldPos.x - waterWorldPos.x) / waterSize + 0.5f;
    uv.y = (worldPos.z - waterWorldPos.z) / waterSize + 0.5f;

    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) return;

    AddRipple(uv, rippleStrength);
}

void InteractiveWater::Update(float deltaTime) {
    rippleTimer += deltaTime;
    if (rippleTimer >= rippleInterval) {
        rippleTimer = 0.0f;
        float u = (float)(rand() % resolution) / (float)resolution;
        float v = (float)(rand() % resolution) / (float)resolution;
        AddRipple(glm::vec2(u, v), 0.05f);
    }
}

void InteractiveWater::GeneratePlane(int subdivisions) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    float step = 1.0f / subdivisions;

    for (int y = 0; y <= subdivisions; y++) {
        for (int x = 0; x <= subdivisions; x++) {
            float fx = x * step;
            float fy = y * step;
            vertices.push_back(fx - 0.5f); // X
            vertices.push_back(0.0f);       // Y
            vertices.push_back(fy - 0.5f); // Z
            vertices.push_back(0.0f);       // Normal X
            vertices.push_back(1.0f);       // Normal Y
            vertices.push_back(0.0f);       // Normal Z
            vertices.push_back(fx);         // UV X
            vertices.push_back(fy);         // UV Y
        }
    }

    for (int y = 0; y < subdivisions; y++) {
        for (int x = 0; x < subdivisions; x++) {
            unsigned int i = y * (subdivisions + 1) + x;
            indices.push_back(i);
            indices.push_back(i + subdivisions + 1);
            indices.push_back(i + 1);
            indices.push_back(i + 1);
            indices.push_back(i + subdivisions + 1);
            indices.push_back(i + subdivisions + 2);
        }
    }

    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glGenBuffers(1, &planeEBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, planeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    planeIndexCount = (unsigned int)indices.size();
}

void InteractiveWater::DrawPlane() {
    glBindVertexArray(planeVAO);
    glDrawElements(GL_TRIANGLES, planeIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void InteractiveWater::SimulateMoss(float deltaTime) {
    glUseProgram(mossShader);

    glUniform1f(glGetUniformLocation(mossShader, "flowRate"),  0.05f);
    glUniform1f(glGetUniformLocation(mossShader, "viscosity"), 0.5f);
    glUniform1f(glGetUniformLocation(mossShader, "deltaTime"), deltaTime);

    glBindImageTexture(0, GetHeightMap(), 0, GL_FALSE, 0, GL_READ_ONLY,  GL_R16F);
    glBindImageTexture(1, mossPing,       0, GL_FALSE, 0, GL_READ_ONLY,  GL_R16F);
    glBindImageTexture(2, mossPong,       0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R16F);

    glDispatchCompute(resolution / 16, resolution / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    std::swap(mossPing, mossPong);
}