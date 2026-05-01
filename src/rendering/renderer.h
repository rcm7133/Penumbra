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
#include "../rendering/effects/clouds/cloudVolumeComponent.h"
#include "shaderLibrary.h"
#include "renderTargets.h"

extern float AMBIENT_MULTIPLIER;
extern bool SKYBOX_ENABLED;

extern bool RENDER_DEBUG_TEXTURE;
extern int  DEBUG_TEXTURE_INDEX;

// GI
extern int GI_MODE;
extern float GI_INTENSITY;

// PATH TRACING
extern int PATH_TRACING_GI_SAMPLES;
extern int PATH_TRACING_GI_BOUNCES;
extern int PATH_TRACING_GI_FACE_SIZE;

// SSR
extern bool SSR_ENABLED;
extern bool REFLECTION_PROBE_ENABLED;
extern float SSR_MIN_STEP_SIZE;
extern float SSR_MAX_STEP_SIZE;
extern int SSR_RAYMARCH_STEPS;

// Clouds
extern bool CLOUD_ENABLED;
extern int CLOUD_LIGHTING_UPDATE_INTERVAL;
extern int CLOUD_RAYMARCH_STEPS;
extern int CLOUD_RESOLUTION_SCALE;
extern int CLOUD_RAYMARCH_LIGHTING_STEPS;
extern float CLOUD_RAYMARCH_LIGHTING_RAY_DEPTH;
extern float CLOUD_ABSORPTION;

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
        rt.fogNoiseTexture = GenerateNoiseTexture(64);
    }
    ~Renderer();

    void InitShadowMaps(std::shared_ptr<Scene> scene);

    void AssignDefaultShader();

    void RenderFrame(float deltaTime);

    [[nodiscard]] unsigned int GetGBufferShader()  const { return shaders.gBuffer; }
    [[nodiscard]] unsigned int GetLightingShader() const { return shaders.lighting; }
    [[nodiscard]] const RenderTargets& GetRenderTargets() const { return rt; }

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

    RenderTargets rt;
    ShaderLibrary shaders;

    std::vector<std::pair<std::shared_ptr<ReflectionProbe>, glm::vec3>> reflectionProbes;

    ScreenQuad quad;

    PTScene ptScene; bool ptSceneBuilt = false;

    // SSBO
    unsigned int probeSSBO = 0;

    // Geometry shader caches
    struct GeometryUniforms {
        int model, view, projection, normalMat;
        int roughness, metallic, diffuseTex, normalMap, hasNormalMap;
    } gBuf;

    struct LightingUniforms {
        int gPosition, gNormal, gAlbedo, cameraPos, ambient;
    } lighting;

    int cloudLightingFrameCounter = 0;

    // Init
    void CreateLitFBO();
    void CreateFogFBO();
    void CreateSSAO();
    void CreateFXAA();
    void CreateSSR();
    void CreateCloudFBO();

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
    void CloudPass(CloudVolume& vol, Light* sun);
    void CloudCompositePass();
    void CloudLightingPass(CloudVolume& vol, Light* sun);

    void RenderDebugTexture();

    void RenderShadowMap(const glm::mat4& lightSpaceMatrix);

    unsigned int RenderCubemapFromPoint(const glm::vec3& position, int faceSize = 64);
    unsigned int PathTraceCubemapFromPoint(const glm::vec3& position, int faceSize = 64);

    void EncodeSH(const float* cubemapData, int faceSize, glm::vec3 outSH[9]);

    std::vector<glm::vec3> ssaoKernelCache;
    static unsigned int GenerateNoiseTexture(int size);
};