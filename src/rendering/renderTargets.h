#pragma once
#include "../config.h"

struct RenderTargets {
    // FBOs
    GLuint litFBO, fogFBO, ssaoFBO, ssaoBlurFBO, fxaaFBO, ssrFBO, ssrCompositeFBO, cloudFBO, cloudCompositeFBO;
    // Textures
    GLuint litTexture, fogTexture, ssaoTexture, ssaoBlurTexture, ssaoNoiseTex,
           fogNoiseTexture, fxaaTexture, ssrTexture, ssrCompositeTexture, cloudTexture, cloudCompositeTexture;
    // RBOs
    GLuint litDepthRBO;

    void Init(int w, int h);
    void Destroy();
};