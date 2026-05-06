#pragma once
#include "imgui.h"
#include "../../../config.h"
#include "rendering/shaderutils.h"

class CloudTextureViewer {
public:
    CloudTextureViewer() = default;
    ~CloudTextureViewer() {  }

    bool IsInitialized() const { return outputTex != 0; }

    void Init(int resolution) {
        Cleanup();
        res = resolution;
        sliceShader = ShaderUtils::LoadComputeShader(
            "../shaders/compute/cloud/textureSliceView.glsl");

        // 2D output texture for ImGui to display
        glGenTextures(1, &outputTex);
        glBindTexture(GL_TEXTURE_2D, outputTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, res, res, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    void UpdateSlice(GLuint sourceTex3D) {
        glBindImageTexture(0, sourceTex3D, 0, GL_TRUE,  0, GL_READ_ONLY,  GL_RGBA16F);
        glBindImageTexture(1, outputTex,   0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

        glUseProgram(sliceShader);
        glUniform1f(glGetUniformLocation(sliceShader, "slice"),   slice);
        glUniform1i(glGetUniformLocation(sliceShader, "channel"), channel);

        int groups = (res + 7) / 8;
        glDispatchCompute(groups, groups, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    void DrawGUI(GLuint sourceTex3D) {
        const char* channels[] = { "R", "G", "B", "A", "RGB" };
        ImGui::SetNextItemWidth(80);
        ImGui::Combo("Channel", &channel, channels, IM_ARRAYSIZE(channels));

        ImGui::SliderFloat("Slice", &slice, 0.0f, 1.0f);

        UpdateSlice(sourceTex3D);

        float panelWidth = ImGui::GetContentRegionAvail().x;
        ImGui::Image((ImTextureID)(intptr_t) outputTex,
            ImVec2(panelWidth, panelWidth),  // square
            ImVec2(0, 1), ImVec2(1, 0));     // flip Y (OpenGL origin)
    }

    void Cleanup() {
        if (outputTex) { glDeleteTextures(1, &outputTex); outputTex = 0; }
    }

private:
    GLuint sliceShader = 0;
    GLuint outputTex   = 0;
    int    res = 64;
    float  slice = 0.5f;
    int    channel = 0;
};
