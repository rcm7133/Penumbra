#include "renderer.h"
#define STB_PERLIN_IMPLEMENTATION
#include "../dependencies/stb_perlin.h"


// Construction and destruction
Renderer::Renderer(int width, int height, const glm::mat4& proj)
    : w(width), h(height), projection(proj), gbuffer(width, height)
{
    CreateLitFBO();
    CreateFogFBO();
    CreateFXAA();
    CreateSSAO();
    fogNoiseTexture = GenerateNoiseTexture(64);
    LoadShaders();
    CacheUniforms();
}

Renderer::~Renderer()
{
    glDeleteFramebuffers(1, &litFBO);
    glDeleteTextures(1, &litTexture);
    glDeleteFramebuffers(1, &fogFBO);
    glDeleteTextures(1, &fogTexture);
    glDeleteFramebuffers(1, &ssaoFBO);
    glDeleteTextures(1, &ssaoTexture);
    glDeleteFramebuffers(1, &ssaoBlurFBO);
    glDeleteTextures(1, &ssaoBlurTexture);
    glDeleteTextures(1, &ssaoNoiseTex);
    glDeleteTextures(1, &fogNoiseTexture);

    glDeleteProgram(gBufferShader);
    glDeleteProgram(lightingShader);
    glDeleteProgram(shadowShader);
    glDeleteProgram(fogShader);
    glDeleteProgram(passthroughShader);
    glDeleteProgram(fogCompositeShader);
    glDeleteProgram(ssaoShader);

    glDeleteFramebuffers(1, &fxaaFBO);
    glDeleteTextures(1, &fxaaTexture);
    glDeleteProgram(fxaaShader);
    glDeleteProgram(ssaoBlurShader);
}

void Renderer::InitShadowMaps(std::shared_ptr<Scene> scene)
{
    int count = 0;
    for (auto& obj : scene->objects)
    {
        if (!obj->light || !obj->light->castsShadow) continue;
        if (count >= MAX_SHADOW_LIGHTS) break;

        auto& light = obj->light;

        glGenFramebuffers(1, &light->shadowFBO);
        glGenTextures(1, &light->shadowMap);
        glBindTexture(GL_TEXTURE_2D, light->shadowMap);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                     SHADOW_RESOLUTION, SHADOW_RESOLUTION,
                     0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        float borderColor[] = {1, 1, 1, 1};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glBindFramebuffer(GL_FRAMEBUFFER, light->shadowFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D, light->shadowMap, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        count++;
    }
}

void Renderer::AssignDefaultShader(std::shared_ptr<Scene> scene)
{
    for (auto& obj : scene->objects)
        if (obj->mesh)
            obj->mesh->material.shader = gBufferShader;
}


void Renderer::CreateLitFBO()
{
    glGenFramebuffers(1, &litFBO);
    glGenTextures(1, &litTexture);
    glBindTexture(GL_TEXTURE_2D, litTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


    glGenRenderbuffers(1, &litDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, litDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);

    glBindFramebuffer(GL_FRAMEBUFFER, litFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, litTexture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, litDepthRBO);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::CreateFogFBO()
{
    glGenFramebuffers(1, &fogFBO);
    glGenTextures(1, &fogTexture);
    glBindTexture(GL_TEXTURE_2D, fogTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                 w / FOG_RESOLUTION_SCALE, h / FOG_RESOLUTION_SCALE,
                 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, fogFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fogTexture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::CreateFXAA()
{
    glGenFramebuffers(1, &fxaaFBO);
    glGenTextures(1, &fxaaTexture);
    glBindTexture(GL_TEXTURE_2D, fxaaTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, fxaaFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fxaaTexture, 0); // was fxaaFBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void Renderer::CreateSSAO()
{
    // Create Kernel
    std::uniform_real_distribution<float> rng(0.0f, 1.0f);
    std::default_random_engine gen;
    std::vector<glm::vec3> kernel;

    for (int i = 0; i < 64; i++)
    {
        glm::vec3 s(rng(gen) * 2.0f - 1.0f,
                     rng(gen) * 2.0f - 1.0f,
                     rng(gen));
        s = glm::normalize(s) * rng(gen);

        float scale = (float)i / 64.0f;
        scale = 0.1f + scale * scale * 0.9f;
        s *= scale;
        kernel.push_back(s);
    }

    // 4x4 noise
    std::vector<glm::vec3> noise;
    for (int i = 0; i < 16; i++)
        noise.emplace_back(rng(gen) * 2.0f - 1.0f, rng(gen) * 2.0f - 1.0f, 0.0f);

    glGenTextures(1, &ssaoNoiseTex);
    glBindTexture(GL_TEXTURE_2D, ssaoNoiseTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, noise.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // SSAO FBO
    glGenFramebuffers(1, &ssaoFBO);
    glGenTextures(1, &ssaoTexture);
    glBindTexture(GL_TEXTURE_2D, ssaoTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoTexture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // SSAO blur FBO
    glGenFramebuffers(1, &ssaoBlurFBO);
    glGenTextures(1, &ssaoBlurTexture);
    glBindTexture(GL_TEXTURE_2D, ssaoBlurTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoBlurTexture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    ssaoKernelCache = std::move(kernel);
}

void Renderer::LoadShaders()
{
    gBufferShader      = ShaderUtils::MakeShaderProgram("../shaders/geometry/geometryVert.glsl",    "../shaders/geometry/geometryFrag.glsl");
    lightingShader     = ShaderUtils::MakeShaderProgram("../shaders/lighting/lightingVert.glsl",     "../shaders/lighting/lightingFrag.glsl");
    shadowShader       = ShaderUtils::MakeShaderProgram("../shaders/shadows/shadowVert.glsl",        "../shaders/shadows/shadowFrag.glsl");
    fogShader          = ShaderUtils::MakeShaderProgram("../shaders/lighting/lightingVert.glsl",     "../shaders/fog/fogFrag.glsl");
    passthroughShader  = ShaderUtils::MakeShaderProgram("../shaders/lighting/lightingVert.glsl",     "../shaders/passthrough/passthroughFrag.glsl");
    fogCompositeShader = ShaderUtils::MakeShaderProgram("../shaders/lighting/lightingVert.glsl",     "../shaders/fog/fogCompositeFrag.glsl");
    ssaoShader         = ShaderUtils::MakeShaderProgram("../shaders/lighting/lightingVert.glsl",     "../shaders/ssao/ssaoFrag.glsl");
    ssaoBlurShader     = ShaderUtils::MakeShaderProgram("../shaders/lighting/lightingVert.glsl",     "../shaders/ssao/ssaoBlurFrag.glsl");
    fxaaShader         = ShaderUtils::MakeShaderProgram("../shaders/lighting/lightingVert.glsl",       "../shaders/fxaa/fxaaFrag.glsl");
    particleLitShader   = ShaderUtils::MakeShaderProgram("../shaders/particles/particleVert.glsl", "../shaders/particles/particleLitFrag.glsl");
    particleUnlitShader = ShaderUtils::MakeShaderProgram("../shaders/particles/particleVert.glsl", "../shaders/particles/particleUnlitFrag.glsl");
}

void Renderer::CacheUniforms()
{
    // Geometry shader
    gBuf_model      = glGetUniformLocation(gBufferShader, "model");
    gBuf_view       = glGetUniformLocation(gBufferShader, "view");
    gBuf_projection = glGetUniformLocation(gBufferShader, "projection");
    gBuf_normalMat  = glGetUniformLocation(gBufferShader, "normalMatrix");
    gBuf_shininess  = glGetUniformLocation(gBufferShader, "shininess");
    gBuf_diffuseTex = glGetUniformLocation(gBufferShader, "diffuseTex");
    gBuf_normalMap  = glGetUniformLocation(gBufferShader, "normalMap");
    gBuf_hasNormalMap = glGetUniformLocation(gBufferShader, "hasNormalMap");

    glUseProgram(gBufferShader);
    glUniform1i(gBuf_diffuseTex, 0);
    glUniform1i(gBuf_normalMap, 1);
    glUniformMatrix4fv(gBuf_projection, 1, GL_FALSE, glm::value_ptr(projection));

    glm::mat4 identity(1.0f);
    glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(identity)));
    glUniformMatrix4fv(gBuf_model, 1, GL_FALSE, glm::value_ptr(identity));
    glUniformMatrix3fv(gBuf_normalMat, 1, GL_FALSE, glm::value_ptr(normalMatrix));

    // Lighting shader
    light_gPosition = glGetUniformLocation(lightingShader, "gPosition");
    light_gNormal   = glGetUniformLocation(lightingShader, "gNormal");
    light_gAlbedo   = glGetUniformLocation(lightingShader, "gAlbedo");
    light_cameraPos = glGetUniformLocation(lightingShader, "cameraPos");
    light_ambient   = glGetUniformLocation(lightingShader, "ambientMultiplier");

    glUseProgram(lightingShader);
    glUniform1i(light_gPosition, 0);
    glUniform1i(light_gNormal, 1);
    glUniform1i(light_gAlbedo, 2);
    glUniform1f(light_ambient, 0.15f);

    // upload SSAO kernel samples
    glUseProgram(ssaoShader);
    for (int i = 0; i < (int)ssaoKernelCache.size(); i++)
    {
        std::string name = "samples[" + std::to_string(i) + "]";
        glUniform3fv(glGetUniformLocation(ssaoShader, name.c_str()), 1,
                     glm::value_ptr(ssaoKernelCache[i]));
    }
    glUniform1i(glGetUniformLocation(ssaoShader, "gPosition"), 0);
    glUniform1i(glGetUniformLocation(ssaoShader, "gNormal"), 1);
    glUniform1i(glGetUniformLocation(ssaoShader, "noiseTexture"), 2);

    ssaoKernelCache.clear();
}

void Renderer::RenderFrame(Camera& camera, std::shared_ptr<Scene> scene, Profiler& profiler)
{
    glm::mat4 view = camera.GetViewMatrix();

    // Pass 0 — shadows
    ShadowPass(scene, profiler);

    // Count shadow lights (used by later passes)
    int shadowCount = 0;
    for (auto& obj : scene->objects)
    {
        if (!obj->light || !obj->light->castsShadow) continue;
        if (shadowCount >= MAX_SHADOW_LIGHTS) break;
        shadowCount++;
    }

    // Pass 1 — geometry
    GeometryPass(view, scene, profiler);

    // Pass 1.5 — SSAO
    if (SSAO_ENABLED)
        SSAOPass(view, profiler);

    // Pass 2 — lighting
    LightingPass(camera, scene, shadowCount, profiler);

    // Pass 3 - particles
    ParticlePass(camera, scene, shadowCount, profiler);

    // Pass 4 — fog (or passthrough)
    if (FOG_ENABLED)
        FogPass(camera, scene, shadowCount, profiler);
    else
        PassthroughPass();

    FXAAPass(profiler);
}

void Renderer::ShadowPass(std::shared_ptr<Scene> scene, Profiler& profiler)
{
    profiler.Begin("Shadow Pass");
    int idx = 0;
    glUseProgram(shadowShader);
    glDisable(GL_CULL_FACE);

    for (auto& obj : scene->objects)
    {
        if (!obj->light || !obj->light->castsShadow) continue;
        if (idx >= MAX_SHADOW_LIGHTS) break;

        auto& light = obj->light;
        glm::mat4 lightProj, lightView;

        if (light->type == LightType::Directional)
        {
            lightProj = glm::ortho(-5.f, 5.f, -2.f, 2.f, 0.1f, 30.f);
            glm::vec3 up = (glm::abs(light->direction.y) > 0.99f)
                ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
            lightView = glm::lookAt(obj->transform.position,
                                    obj->transform.position + light->direction, up);
        }
        else if (light->type == LightType::Spot)
        {
            lightProj = glm::perspective(glm::acos(light->outerCutoff) * 2.0f,
                                         1.0f, 0.1f, 50.f);
            glm::vec3 up = (glm::abs(light->direction.y) > 0.99f)
                ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
            lightView = glm::lookAt(obj->transform.position,
                                    obj->transform.position + light->direction, up);
        }

        light->lightSpaceMatrix = lightProj * lightView;

        glViewport(0, 0, SHADOW_RESOLUTION, SHADOW_RESOLUTION);
        glBindFramebuffer(GL_FRAMEBUFFER, light->shadowFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        RenderShadowMap(light->lightSpaceMatrix, scene);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        idx++;
    }

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    profiler.End("Shadow Pass");
}

void Renderer::GeometryPass(const glm::mat4& view, std::shared_ptr<Scene> scene, Profiler& profiler)
{
    profiler.Begin("Geometry Pass");
    glViewport(0, 0, w, h);
    gbuffer.Bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glUseProgram(gBufferShader);
    glUniformMatrix4fv(gBuf_view, 1, GL_FALSE, glm::value_ptr(view));
    scene->RenderGeometry(gBufferShader, gBuf_model, gBuf_shininess);
    gbuffer.Unbind();
    profiler.End("Geometry Pass");
}

void Renderer::SSAOPass(const glm::mat4& view, Profiler& profiler)
{
    // SSAO
    profiler.Begin("SSAO Pass");
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(ssaoShader);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gbuffer.gPosition);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gbuffer.gNormal);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, ssaoNoiseTex);

    glUniformMatrix4fv(glGetUniformLocation(ssaoShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(ssaoShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniform2f(glGetUniformLocation(ssaoShader, "screenSize"), (float)w, (float)h);
    glUniform1f(glGetUniformLocation(ssaoShader, "radius"), SSAO_RADIUS);
    glUniform1f(glGetUniformLocation(ssaoShader, "bias"), SSAO_BIAS);
    glUniform1i(glGetUniformLocation(ssaoShader, "kernelSize"), SSAO_KERNEL_SIZE);

    quad.Draw();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    profiler.End("SSAO Pass");

    // Blur
    profiler.Begin("SSAO Blur");
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(ssaoBlurShader);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, ssaoTexture);
    glUniform1i(glGetUniformLocation(ssaoBlurShader, "ssaoInput"), 0);

    quad.Draw();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    profiler.End("SSAO Blur");
}

void Renderer::LightingPass(Camera& camera, std::shared_ptr<Scene> scene, int shadowCount, Profiler& profiler)
{
    profiler.Begin("Lighting Pass");
    glBindFramebuffer(GL_FRAMEBUFFER, litFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(lightingShader);

    glUniform1i(glGetUniformLocation(lightingShader, "pcfKernelSize"), PCF_KERNEL_SIZE);
    glUniform1i(glGetUniformLocation(lightingShader, "shadowLightCount"), shadowCount);
    glUniform1i(glGetUniformLocation(lightingShader, "pcfEnabled"), PCF_ENABLED ? 1 : 0);
    glUniform1f(glGetUniformLocation(lightingShader, "shadowBias"), SHADOW_BIAS);
    glUniform1f(glGetUniformLocation(lightingShader, "shadowNormalOffset"), SHADOW_NORMAL_OFFSET);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gbuffer.gPosition);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gbuffer.gNormal);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, gbuffer.gDiffuse);

    // Shadow maps on slots 3+
    int si = 0;
    for (auto& obj : scene->objects)
    {
        if (!obj->light || !obj->light->castsShadow) continue;
        if (si >= MAX_SHADOW_LIGHTS) break;

        glActiveTexture(GL_TEXTURE3 + si);
        glBindTexture(GL_TEXTURE_2D, obj->light->shadowMap);

        std::string name = "shadowMap[" + std::to_string(si) + "]";
        glUniform1i(glGetUniformLocation(lightingShader, name.c_str()), 3 + si);

        std::string lsm = "lightSpaceMatrix[" + std::to_string(si) + "]";
        glUniformMatrix4fv(glGetUniformLocation(lightingShader, lsm.c_str()),
                           1, GL_FALSE, glm::value_ptr(obj->light->lightSpaceMatrix));
        si++;
    }

    // SSAO on next available slot
    int ssaoSlot = 3 + si;
    glActiveTexture(GL_TEXTURE0 + ssaoSlot);
    glBindTexture(GL_TEXTURE_2D, SSAO_ENABLED ? ssaoBlurTexture : 0);
    glUniform1i(glGetUniformLocation(lightingShader, "ssaoTexture"), ssaoSlot);
    glUniform1i(glGetUniformLocation(lightingShader, "ssaoEnabled"), SSAO_ENABLED ? 1 : 0);

    glUniform3f(light_cameraPos,
                camera.transform.position.x,
                camera.transform.position.y,
                camera.transform.position.z);

    scene->UploadLights(lightingShader);
    quad.Draw();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    profiler.End("Lighting Pass");
}

void Renderer::FogPass(Camera& camera, std::shared_ptr<Scene> scene, int shadowCount, Profiler& profiler)
{
    // Volumetric fog
    profiler.Begin("Fog Pass");
    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, fogFBO);
    glViewport(0, 0, w / FOG_RESOLUTION_SCALE, h / FOG_RESOLUTION_SCALE);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(fogShader);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, litTexture);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gbuffer.gPosition);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, fogNoiseTexture);

    glUniform1i(glGetUniformLocation(fogShader, "litScene"), 0);
    glUniform1i(glGetUniformLocation(fogShader, "gPosition"), 1);
    glUniform1i(glGetUniformLocation(fogShader, "noiseTexture"), 2);
    glUniform1f(glGetUniformLocation(fogShader, "fogScale"), FOG_SCALE);
    glUniform1f(glGetUniformLocation(fogShader, "fogScrollSpeed"), FOG_SCROLL_SPEED);

    int si = 0;
    for (auto& obj : scene->objects)
    {
        if (!obj->light || !obj->light->castsShadow) continue;
        if (si >= MAX_SHADOW_LIGHTS) break;

        glActiveTexture(GL_TEXTURE3 + si);
        glBindTexture(GL_TEXTURE_2D, obj->light->shadowMap);
        std::string name = "shadowMap[" + std::to_string(si) + "]";
        glUniform1i(glGetUniformLocation(fogShader, name.c_str()), 3 + si);
        std::string lsm = "lightSpaceMatrix[" + std::to_string(si) + "]";
        glUniformMatrix4fv(glGetUniformLocation(fogShader, lsm.c_str()),
                           1, GL_FALSE, glm::value_ptr(obj->light->lightSpaceMatrix));
        si++;
    }

    glUniform3f(glGetUniformLocation(fogShader, "cameraPos"),
                camera.transform.position.x,
                camera.transform.position.y,
                camera.transform.position.z);
    glUniform1f(glGetUniformLocation(fogShader, "time"), (float)glfwGetTime());
    glUniform1f(glGetUniformLocation(fogShader, "fogDensity"), FOG_DENSITY);
    glUniform1i(glGetUniformLocation(fogShader, "steps"), FOG_STEPS);
    glUniform1i(glGetUniformLocation(fogShader, "shadowLightCount"), shadowCount);

    scene->UploadLights(fogShader);
    quad.Draw();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    profiler.End("Fog Pass");

    // Composite
    profiler.Begin("Fog Composite Pass");
    glBindFramebuffer(GL_FRAMEBUFFER, fxaaFBO);
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(fogCompositeShader);
    glBindFramebuffer(GL_FRAMEBUFFER, fxaaFBO);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, litTexture);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, fogTexture);
    glUniform1i(glGetUniformLocation(fogCompositeShader, "litScene"), 0);
    glUniform1i(glGetUniformLocation(fogCompositeShader, "fogBuffer"), 1);
    glUniform2f(glGetUniformLocation(fogCompositeShader, "resolution"), (float)w, (float)h);
    glUniform2f(glGetUniformLocation(fogCompositeShader, "fogResolution"),
                (float)(w / FOG_RESOLUTION_SCALE), (float)(h / FOG_RESOLUTION_SCALE));
    glUniform1i(glGetUniformLocation(fogCompositeShader, "fogBlurKernelSize"), FOG_BLUR_KERNEL_SIZE);

    quad.Draw();
    glEnable(GL_DEPTH_TEST);
    profiler.End("Fog Composite Pass");
}

void Renderer::PassthroughPass()
{
    glBindFramebuffer(GL_FRAMEBUFFER, fxaaFBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(passthroughShader);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, litTexture);
    glUniform1i(glGetUniformLocation(passthroughShader, "litScene"), 0);
    quad.Draw();
}

void Renderer::FXAAPass(Profiler& profiler)
{
    profiler.Begin("FXAA Pass");

    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (FXAA_ENABLED)
    {
        glUseProgram(fxaaShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fxaaTexture);
        glUniform1i(glGetUniformLocation(fxaaShader, "screenTexture"), 0);
        glUniform2f(glGetUniformLocation(fxaaShader, "resolution"), (float)w, (float)h);
        glUniform1f(glGetUniformLocation(fxaaShader, "edgeThreshold"), FXAA_EDGE_THRESHOLD);
        glUniform1f(glGetUniformLocation(fxaaShader, "edgeThresholdMin"), FXAA_EDGE_THRESHOLD_MIN);
    }
    else
    {
        glUseProgram(passthroughShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fxaaTexture);
        glUniform1i(glGetUniformLocation(passthroughShader, "litScene"), 0);
    }

    quad.Draw();
    glEnable(GL_DEPTH_TEST);
    profiler.End("FXAA Pass");
}

void Renderer::ParticlePass(Camera& camera, std::shared_ptr<Scene> scene, int shadowCount, Profiler& profiler)
{
    profiler.Begin("Particle Pass");
    glBindFramebuffer(GL_FRAMEBUFFER, litFBO);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, gbuffer.FBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, litFBO);
    glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, litFBO);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(0x8861); // GL_POINT_SPRITE constant removed from core headers

    glm::mat4 view = camera.GetViewMatrix();

    for (auto& obj: scene->objects)
    {
        if (!obj->particleSystem || !obj->enabled)
            continue;

        auto& ps = obj->particleSystem;
        unsigned int shader = ps->IsLit() ? particleLitShader : particleUnlitShader;
        glUseProgram(shader);

        // Set shader uniforms
        glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3f(glGetUniformLocation(shader, "cameraPos"), camera.transform.position.x, camera.transform.position.y, camera.transform.position.z);

        // Bind lighting and shadow map data if the particles are lit
        if (ps->IsLit())
        {
            int shadowIndex = 0;
            // Get shadow lights for shadow maps
            for (auto& potentialShadowLight : scene->objects) {
                // Make sure candidate is on, it has a light, and can cast shadows
                if (!potentialShadowLight->light || !potentialShadowLight->enabled || !potentialShadowLight->light->castsShadow)
                    continue;
                // stop if the number of shadow lights is more than or = the max
                if (shadowIndex >= MAX_SHADOW_LIGHTS) break;
                // Get Shadow map from light
                glActiveTexture(GL_TEXTURE0 + shadowIndex);
                glBindTexture(GL_TEXTURE_2D, potentialShadowLight->light->shadowMap);
                // Set shadow map index uniform
                std::string name = "shadowMap[" + std::to_string(shadowIndex) + "]";
                glUniform1i(glGetUniformLocation(shader, name.c_str()), shadowIndex);
                // Let light space matrix uniform
                std::string lsm = "lightSpaceMatrix[" + std::to_string(shadowIndex) + "]";
                glUniformMatrix4fv(glGetUniformLocation(shader, lsm.c_str()),
                                   1, GL_FALSE, glm::value_ptr(potentialShadowLight->light->lightSpaceMatrix));
            }

            glUniform1i(glGetUniformLocation(shader, "shadowLightCount"), shadowCount);
            scene->UploadLights(shader);
        }

        // Bind particle SSBO and draw particle system
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ps->GetSSBO());
        glBindVertexArray(ps->GetDummyVAO());
        glDrawArrays(GL_POINTS, 0, ps->GetCount());
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);

    profiler.End("Particle Pass");
}


void Renderer::RenderShadowMap(const glm::mat4& lightSpaceMatrix, std::shared_ptr<Scene> scene)
{
    glUseProgram(shadowShader);
    glUniformMatrix4fv(glGetUniformLocation(shadowShader, "lightSpaceMatrix"),
                       1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

    for (auto& obj : scene->objects)
    {
        if (!obj->mesh || !obj->enabled) continue;
        glm::mat4 model = obj->transform.GetMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shadowShader, "model"),
                           1, GL_FALSE, glm::value_ptr(model));
        obj->mesh->Draw();
    }
}

// 3D Perlin noise texture
unsigned int Renderer::GenerateNoiseTexture(int size)
{
    std::vector<float> data(size * size * size);
    for (int z = 0; z < size; z++)
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++)
            {
                float fx = (float)x / size;
                float fy = (float)y / size;
                float fz = (float)z / size;

                float n = 0.0f;
                n += 1.000f * stb_perlin_noise3(fx * 2,  fy * 2,  fz * 2,  2,  2,  2);
                n += 0.500f * stb_perlin_noise3(fx * 4,  fy * 4,  fz * 4,  4,  4,  4);
                n += 0.250f * stb_perlin_noise3(fx * 8,  fy * 8,  fz * 8,  8,  8,  8);
                n += 0.125f * stb_perlin_noise3(fx * 16, fy * 16, fz * 16, 16, 16, 16);

                n = glm::clamp(n * 0.5f + 0.5f, 0.0f, 1.0f);
                data[z * size * size + y * size + x] = n;
            }

    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_3D, tex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, size, size, size, 0, GL_RED, GL_FLOAT, data.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return tex;
}

// Mode: 0=Position, 1=Normals, 2=Diffuse, 3=SSAO, 4=SSAO Blur,
//       5=Lighting, 6-8=Shadow Maps, 9=Fog

unsigned int Renderer::GetDebugTexture(int mode, std::shared_ptr<Scene> scene) const
{
    switch (mode)
    {
        case 0: return gbuffer.gPosition;
        case 1: return gbuffer.gNormal;
        case 2: return gbuffer.gDiffuse;
        case 3: return ssaoTexture;
        case 4: return ssaoBlurTexture;
        case 5: return litTexture;
        case 6:
        case 7:
        case 8:
        {
            int idx = 0;
            for (auto& obj : scene->objects)
            {
                if (!obj->light || !obj->light->castsShadow) continue;
                if (idx == mode - 6) return obj->light->shadowMap;
                idx++;
            }
            return 0;
        }
        case 9: return fogTexture;
        default: return 0;
    }
}
