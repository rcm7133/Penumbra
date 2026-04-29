#pragma once
#include "../config.h"

struct ShaderLibrary {
    GLuint gBuffer, lighting, shadow, fog, fogComposite, passthrough;
    GLuint ssao, ssaoBlur, fxaa;
    GLuint particleLit, particleUnlit, pointShadow;
    GLuint skybox, probeBake;
    GLuint ssr, ssrComposite;
    GLuint cloud, cloudComposite, cloudLighting;

    void Load();
    void Destroy();
};