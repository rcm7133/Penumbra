#pragma once

#include "../config.h"
#include "gbuffer.h"
#include "quad.h"
#include "../scene.h"
#include "camera.h"
#include "../utils/profiler.h"
#include "shaderutils.h"
#include "../components/meshComponent.h"
#include "../components/lightComponent.h"

// Shadow settings
extern int SHADOW_RESOLUTION;
extern int POINT_SHADOW_RESOLUTION;
extern float POINT_SHADOW_FAR_PLANE;
extern int PCF_KERNEL_SIZE;
extern bool PCF_ENABLED;
extern int MAX_SHADOW_LIGHTS;
extern float SHADOW_BIAS;
extern float SHADOW_NORMAL_OFFSET;

// Fog settings
extern bool FOG_ENABLED;
extern float FOG_DENSITY;
extern int FOG_STEPS;
extern float FOG_SCALE;
extern float FOG_SCROLL_SPEED;
extern int FOG_RESOLUTION_SCALE;
extern int FOG_BLUR_KERNEL_SIZE;

// SSAO settings
extern bool SSAO_ENABLED;
extern float SSAO_RADIUS;
extern float SSAO_BIAS;
extern int SSAO_KERNEL_SIZE;

// FXAA
extern bool FXAA_ENABLED;
extern float FXAA_EDGE_THRESHOLD;
extern float FXAA_EDGE_THRESHOLD_MIN;

class Renderer
{
public:
    Renderer(int width, int height, const glm::mat4& projection);
    ~Renderer();

    void InitShadowMaps(std::shared_ptr<Scene> scene);

    void AssignDefaultShader(std::shared_ptr<Scene> scene);

    void RenderFrame(Camera& camera, std::shared_ptr<Scene> scene, Profiler& profiler);

    unsigned int GetGBufferShader()  const { return gBufferShader; }
    unsigned int GetLightingShader() const { return lightingShader; }
    unsigned int GetDebugTexture(int mode, std::shared_ptr<Scene> scene) const;

private:
    int w, h;
    glm::mat4 projection;

    GBuffer gbuffer;
    ScreenQuad quad;

    // FBOs and textures
    unsigned int litFBO;
    unsigned int litTexture;
    unsigned int fogFBO;
    unsigned int fogTexture;
    unsigned int ssaoFBO;
    unsigned int ssaoTexture;
    unsigned int ssaoBlurFBO;
    unsigned int ssaoBlurTexture;
    unsigned int ssaoNoiseTex;
    unsigned int fogNoiseTexture;
    unsigned int fxaaFBO;
    unsigned int fxaaTexture;
    unsigned int litDepthRBO;

    // Shaders
    unsigned int gBufferShader;
    unsigned int lightingShader;
    unsigned int shadowShader;
    unsigned int fogShader;
    unsigned int passthroughShader;
    unsigned int fogCompositeShader;
    unsigned int ssaoShader;
    unsigned int ssaoBlurShader;
    unsigned int fxaaShader;
    unsigned int particleLitShader;
    unsigned int particleUnlitShader;
    unsigned int pointShadowShader;
    unsigned int skyboxShader;

    // Geometry shader caches
    int gBuf_model, gBuf_view, gBuf_projection, gBuf_normalMat;
    int gBuf_shininess, gBuf_diffuseTex, gBuf_normalMap, gBuf_hasNormalMap;

    // Lighting shader caches
    int light_gPosition, light_gNormal, light_gAlbedo, light_cameraPos, light_ambient;

    // Init
    void CreateLitFBO();
    void CreateFogFBO();
    void CreateSSAO();
    void CreateFXAA();
    void LoadShaders();
    void CacheUniforms();

    // Passes
    void ShadowPass(std::shared_ptr<Scene> scene, Profiler& profiler);
    void GeometryPass(const glm::mat4& view, std::shared_ptr<Scene> scene, Profiler& profiler);
    void SSAOPass(const glm::mat4& view, Profiler& profiler);
    void LightingPass(Camera& camera, std::shared_ptr<Scene> scene, int shadowCount, Profiler& profiler);
    void FogPass(Camera& camera, std::shared_ptr<Scene> scene, int shadowCount, Profiler& profiler);
    void PassthroughPass();
    void FXAAPass(Profiler& profiler);
    void ParticlePass(Camera& camera, std::shared_ptr<Scene> scene, int shadowCount, Profiler& profiler);
    void SkyboxPass(Camera& camera, std::shared_ptr<Scene> scene);

    void RenderShadowMap(const glm::mat4& lightSpaceMatrix, std::shared_ptr<Scene> scene);

    std::vector<glm::vec3> ssaoKernelCache;
    static unsigned int GenerateNoiseTexture(int size);
};