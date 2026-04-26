#pragma once

#include "../config.h"
#include "gbuffer.h"
#include "quad.h"
#include "../scene.h"
#include "../physics/camera/camera.h"
#include "../utils/profiler.h"
#include "shaderutils.h"
#include "mesh/meshComponent.h"
#include "effects/lights/lightComponent.h"
#include "../rendering/effects/water/interactiveWaterComponent.h"
#include "rendering/pathTracing/pathTracer.h"
#include "../rendering/effects/reflections/reflectionProbe.h"

extern float AMBIENT_MULTIPLIER;
extern bool SKYBOX_ENABLED;

// GI
extern int GI_MODE;
extern float GI_INTENSITY;

// PT
extern int PATH_TRACING_GI_SAMPLES;
extern int PATH_TRACING_GI_BOUNCES;
extern int PATH_TRACING_GI_FACE_SIZE;

// SSR
extern bool SSR_ENABLED;
extern bool REFLECTION_PROBE_ENABLED;
extern float SSR_MIN_STEP_SIZE;
extern float SSR_MAX_STEP_SIZE;
extern int SSR_RAYMARCH_STEPS;

// Reflection Probes
extern int MAX_REFLECTION_PROBES;

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
    Renderer(int width, int height, const glm::mat4& projection,
        std::shared_ptr<Scene> scene, Camera& camera, Profiler& profiler)
            : w(width), h(height), projection(projection), gbuffer(width, height), scene(std::move(scene)), camera(camera), profiler(profiler) {
        CreateFBOS();
        LoadShaders();
        CacheUniforms();
        fogNoiseTexture = GenerateNoiseTexture(64);
    }
    ~Renderer();

    void InitShadowMaps(std::shared_ptr<Scene> scene);

    void AssignDefaultShader();

    void RenderFrame(float deltaTime);

    [[nodiscard]] unsigned int GetGBufferShader()  const { return gBufferShader; }
    [[nodiscard]] unsigned int GetLightingShader() const { return lightingShader; }
    [[nodiscard]] unsigned int GetDebugTexture(int mode, std::shared_ptr<Scene> scene) const;

    void BakeLightProbes();
    void ClearProbes();
    void UploadProbeData(const ProbeGrid& grid);

    void CollectReflectionProbes();
    void BakeReflectionProbes();
    bool HasReflectionProbes() const { return !reflectionProbes.empty(); }
private:
    int w, h;
    glm::mat4 projection;
    GBuffer gbuffer;
    std::shared_ptr<Scene> scene;
    Camera& camera;
    Profiler& profiler;

    std::vector<std::pair<std::shared_ptr<ReflectionProbe>, glm::vec3>> reflectionProbes;

    ScreenQuad quad;

    PTScene ptScene;
    bool ptSceneBuilt = false;

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
    unsigned int ssrFBO;
    unsigned int ssrTexture;
    unsigned int ssrCompositeFBO;
    unsigned int ssrCompositeTexture;

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
    unsigned int probeBakeShader;
    unsigned int ssrShader;
    unsigned int ssrCompositeShader;

    // SSBO
    unsigned int probeSSBO = 0;

    // Geometry shader caches
    int gBuf_model, gBuf_view, gBuf_projection, gBuf_normalMat;
    int gBuf_roughness, gBuf_metallic, gBuf_diffuseTex, gBuf_normalMap, gBuf_hasNormalMap;

    // Lighting shader caches
    int light_gPosition, light_gNormal, light_gAlbedo, light_cameraPos, light_ambient;

    // Init
    void CreateLitFBO();
    void CreateFogFBO();
    void CreateSSAO();
    void CreateFXAA();
    void CreateSSR();

    void LoadShaders();
    void CacheUniforms();
    void CreateFBOS();

    // Passes
    void ShadowPass(bool staticOnly = false);
    void GeometryPass(const glm::mat4& view);
    void SSAOPass(const glm::mat4& view);
    void SSRPass();
    void SSRCompositePass();
    void LightingPass(int shadowCount);
    void FogPass(int shadowCount);
    void PassthroughPass();
    void FXAAPass();
    void ParticlePass(int shadowCount);
    void SkyboxPass();
    void WaterPass(float deltaTime);

    void RenderShadowMap(const glm::mat4& lightSpaceMatrix);

    unsigned int RenderCubemapFromPoint(const glm::vec3& position, int faceSize = 64);
    unsigned int PathTraceCubemapFromPoint(const glm::vec3& position, int faceSize = 64);

    void EncodeSH(const float* cubemapData, int faceSize, glm::vec3 outSH[9]);

    std::vector<glm::vec3> ssaoKernelCache;
    static unsigned int GenerateNoiseTexture(int size);
};