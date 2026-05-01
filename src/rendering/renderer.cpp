    #include "renderer.h"
    #define STB_PERLIN_IMPLEMENTATION
    #include "../../dependencies/stb_perlin.h"
    #include "effects/particles/particleSystemComponent.h"
    #include "effects/reflections/reflectionProbeComponent.h"


    Renderer::~Renderer()
    {
        glDeleteFramebuffers(1, &rt.litFBO);
        glDeleteTextures(1, &rt.litTexture);
        glDeleteFramebuffers(1, &rt.fogFBO);
        glDeleteTextures(1, &rt.fogTexture);
        glDeleteFramebuffers(1, &rt.ssaoFBO);
        glDeleteTextures(1, &rt.ssaoTexture);
        glDeleteFramebuffers(1, &rt.ssaoBlurFBO);
        glDeleteTextures(1, &rt.ssaoBlurTexture);
        glDeleteTextures(1, &rt.ssaoNoiseTex);
        glDeleteTextures(1, &rt.fogNoiseTexture);

        glDeleteProgram(shaders.gBuffer);
        glDeleteProgram(shaders.lighting);
        glDeleteProgram(shaders.shadow);
        glDeleteProgram(shaders.fog);
        glDeleteProgram(shaders.passthrough);
        glDeleteProgram(shaders.fogComposite);
        glDeleteProgram(shaders.ssao);
        glDeleteProgram(shaders.skybox);
        glDeleteProgram(shaders.fxaa);

        glDeleteFramebuffers(1, &rt.fxaaFBO);
        glDeleteTextures(1, &rt.fxaaTexture);
        glDeleteProgram(shaders.fxaa);
        glDeleteProgram(shaders.ssaoBlur);
        glDeleteProgram(shaders.cloud);
        glDeleteTextures(1, &rt.cloudTexture);
    }

    void Renderer::LoadShaders() {
        shaders.gBuffer = ShaderUtils::MakeShaderProgram("../shaders/geometry/geometryVert.glsl",    "../shaders/geometry/geometryFrag.glsl");
        shaders.lighting = ShaderUtils::MakeShaderProgram("../shaders/lighting/lightingVert.glsl",     "../shaders/lighting/lightingFrag.glsl");
        shaders.shadow = ShaderUtils::MakeShaderProgram("../shaders/shadows/shadowVert.glsl",        "../shaders/shadows/shadowFrag.glsl");
        shaders.fog = ShaderUtils::MakeShaderProgram("../shaders/lighting/lightingVert.glsl",     "../shaders/fog/fogFrag.glsl");
        shaders.passthrough = ShaderUtils::MakeShaderProgram("../shaders/lighting/lightingVert.glsl",     "../shaders/passthrough/passthroughFrag.glsl");
        shaders.fogComposite = ShaderUtils::MakeShaderProgram("../shaders/lighting/lightingVert.glsl",     "../shaders/fog/fogCompositeFrag.glsl");
        shaders.ssao = ShaderUtils::MakeShaderProgram("../shaders/lighting/lightingVert.glsl",     "../shaders/ssao/ssaoFrag.glsl");
        shaders.ssaoBlur = ShaderUtils::MakeShaderProgram("../shaders/lighting/lightingVert.glsl",     "../shaders/ssao/ssaoBlurFrag.glsl");
        shaders.fxaa = ShaderUtils::MakeShaderProgram("../shaders/lighting/lightingVert.glsl",       "../shaders/fxaa/fxaaFrag.glsl");
        shaders.particleLit = ShaderUtils::MakeShaderProgram("../shaders/particles/particleVert.glsl", "../shaders/particles/particleLitFrag.glsl");
        shaders.particleUnlit = ShaderUtils::MakeShaderProgram("../shaders/particles/particleVert.glsl", "../shaders/particles/particleUnlitFrag.glsl");
        shaders.pointShadow = ShaderUtils::MakeShaderProgram("../shaders/shadows/pointShadowVert.glsl", "../shaders/shadows/pointShadowFrag.glsl");
        shaders.skybox = ShaderUtils::MakeShaderProgram("../shaders/skybox/skyboxVert.glsl","../shaders/skybox/skyboxFrag.glsl");
        shaders.probeBake = ShaderUtils::MakeShaderProgram("../shaders/lighting/probeBakeVert.glsl", "../shaders/lighting/probeBakeFrag.glsl");
        shaders.ssr = ShaderUtils::MakeShaderProgram("../shaders/ssr/ssrVert.glsl", "../shaders/ssr/ssrFrag.glsl");
        shaders.ssrComposite = ShaderUtils::MakeShaderProgram("../shaders/ssr/ssrVert.glsl", "../shaders/ssr/ssrCompositeFrag.glsl");
        shaders.cloud= ShaderUtils::MakeShaderProgram("../shaders/lighting/lightingVert.glsl",     "../shaders/clouds/cloudFrag.glsl");
        shaders.cloudComposite = ShaderUtils::MakeShaderProgram("../shaders/lighting/lightingVert.glsl",     "../shaders/clouds/cloudComposite.glsl");
        shaders.cloudLighting = ShaderUtils::LoadComputeShader("../shaders/compute/cloud/lightingVoxelCompute.glsl");
    }

    void Renderer::CreateFBOS() {
        CreateLitFBO();
        CreateFogFBO();
        CreateFXAA();
        CreateSSAO();
        CreateSSR();
        CreateCloudFBO();
    }


    void Renderer::InitShadowMaps(std::shared_ptr<Scene> scene)
    {
        int count = 0;
        for (auto& obj : scene->objects)
        {
            auto lc = obj->GetComponent<LightComponent>();
            if (!lc || !lc->light->castsShadow) continue;
            if (count >= MAX_SHADOW_LIGHTS) break;

            auto& light = lc->light;

            if (light->type == LightType::Point)
            {
                glGenFramebuffers(1, &light->shadowCubeFBO);

                // Color cubemap (stores linear depth)
                glGenTextures(1, &light->shadowCubemap);
                glBindTexture(GL_TEXTURE_CUBE_MAP, light->shadowCubemap);
                for (int i = 0; i < 6; i++)
                {
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_R32F,
                                 POINT_SHADOW_RESOLUTION, POINT_SHADOW_RESOLUTION,
                                 0, GL_RED, GL_FLOAT, NULL);
                }
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

                // Depth cubemap (must also be layered for geometry shader)
                glGenTextures(1, &light->shadowCubeDepthRBO);
                glBindTexture(GL_TEXTURE_CUBE_MAP, light->shadowCubeDepthRBO);
                for (int i = 0; i < 6; i++)
                {
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT24,
                                 POINT_SHADOW_RESOLUTION, POINT_SHADOW_RESOLUTION,
                                 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
                }
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

                // Attach both as layered

                glBindFramebuffer(GL_FRAMEBUFFER, light->shadowCubeFBO);
                glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, light->shadowCubemap, 0);
                glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, light->shadowCubeDepthRBO, 0);

                GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                if (status != GL_FRAMEBUFFER_COMPLETE)
                    std::cerr << "Point shadow FBO INCOMPLETE: 0x" << std::hex << status << std::dec << std::endl;
                else
                    std::cerr << "Point shadow FBO complete" << std::endl;

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
            else
            {
                // 2D shadow map
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
            }

            count++;
        }
    }

    void Renderer::AssignDefaultShader()
    {
        for (auto& obj : scene->objects)
        {
            auto mc = obj->GetComponent<MeshComponent>();
            if (mc) {
                for (auto& mat : mc->model->materials)
                    mat.shader = shaders.gBuffer;
            }
        }
    }

    void Renderer::CreateLitFBO()
    {
        glGenFramebuffers(1, &rt.litFBO);
        glGenTextures(1, &rt.litTexture);
        glBindTexture(GL_TEXTURE_2D, rt.litTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


        glGenRenderbuffers(1, &rt.litDepthRBO);
        glBindRenderbuffer(GL_RENDERBUFFER, rt.litDepthRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);

        glBindFramebuffer(GL_FRAMEBUFFER, rt.litFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.litTexture, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt.litDepthRBO);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Renderer::CreateFogFBO()
    {
        glGenFramebuffers(1, &rt.fogFBO);
        glGenTextures(1, &rt.fogTexture);
        glBindTexture(GL_TEXTURE_2D, rt.fogTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                     w / FOG_RESOLUTION_SCALE, h / FOG_RESOLUTION_SCALE,
                     0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, rt.fogFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.fogTexture, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Renderer::CreateCloudFBO() {
        glGenFramebuffers(1, &rt.cloudFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, rt.cloudFBO);

        glGenTextures(1, &rt.cloudTexture);
        glBindTexture(GL_TEXTURE_2D, rt.cloudTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
        w / CLOUD_RESOLUTION_SCALE, h / CLOUD_RESOLUTION_SCALE,
        0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.cloudTexture, 0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "Cloud FBO incomplete!" << std::endl;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glGenFramebuffers(1, &rt.cloudCompositeFBO);
        glGenTextures(1, &rt.cloudCompositeTexture);
        glBindTexture(GL_TEXTURE_2D, rt.cloudCompositeTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, rt.cloudCompositeFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.cloudCompositeTexture, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Renderer::CreateSSR() {
        // SSR FBO
        glGenFramebuffers(1, &rt.ssrFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, rt.ssrFBO);

        glGenTextures(1, &rt.ssrTexture);
        glBindTexture(GL_TEXTURE_2D, rt.ssrTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.ssrTexture, 0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "SSR FBO incomplete!" << std::endl;

        // SSR Composite
        glGenFramebuffers(1, &rt.ssrCompositeFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, rt.ssrCompositeFBO);

        glGenTextures(1, &rt.ssrCompositeTexture);
        glBindTexture(GL_TEXTURE_2D, rt.ssrCompositeTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.ssrCompositeTexture, 0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "SSR Composite FBO incomplete!" << std::endl;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Renderer::CreateFXAA() {
        glGenFramebuffers(1, &rt.fxaaFBO);
        glGenTextures(1, &rt.fxaaTexture);
        glBindTexture(GL_TEXTURE_2D, rt.fxaaTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, rt.fxaaFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.fxaaTexture, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "FXAA FBO incomplete!" << std::endl;
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

        glGenTextures(1, &rt.ssaoNoiseTex);
        glBindTexture(GL_TEXTURE_2D, rt.ssaoNoiseTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, noise.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        // SSAO FBO
        glGenFramebuffers(1, &rt.ssaoFBO);
        glGenTextures(1, &rt.ssaoTexture);
        glBindTexture(GL_TEXTURE_2D, rt.ssaoTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, rt.ssaoFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.ssaoTexture, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // SSAO blur FBO
        glGenFramebuffers(1, &rt.ssaoBlurFBO);
        glGenTextures(1, &rt.ssaoBlurTexture);
        glBindTexture(GL_TEXTURE_2D, rt.ssaoBlurTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, rt.ssaoBlurFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.ssaoBlurTexture, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        ssaoKernelCache = std::move(kernel);
    }

    void Renderer::CacheUniforms()
    {
        // Geometry shader
        gBuf.model      = glGetUniformLocation(shaders.gBuffer, "model");
        gBuf.view       = glGetUniformLocation(shaders.gBuffer, "view");
        gBuf.projection = glGetUniformLocation(shaders.gBuffer, "projection");
        gBuf.normalMat  = glGetUniformLocation(shaders.gBuffer, "normalMatrix");
        gBuf.roughness = glGetUniformLocation(shaders.gBuffer, "roughness");
        gBuf.metallic  = glGetUniformLocation(shaders.gBuffer, "metallic");
        gBuf.diffuseTex = glGetUniformLocation(shaders.gBuffer, "diffuseTex");
        gBuf.normalMap  = glGetUniformLocation(shaders.gBuffer, "normalMap");
        gBuf.hasNormalMap = glGetUniformLocation(shaders.gBuffer, "hasNormalMap");

        glUseProgram(shaders.gBuffer);
        glUniform1i(gBuf.diffuseTex, 0);
        glUniform1i(gBuf.normalMap, 1);
        glUniform1i(glGetUniformLocation(shaders.gBuffer, "heightMap"), 2);
        glUniform1i(glGetUniformLocation(shaders.gBuffer, "metallicRoughnessMap"), 3);
        glUniformMatrix4fv(gBuf.projection, 1, GL_FALSE, glm::value_ptr(projection));

        glm::mat4 identity(1.0f);
        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(identity)));
        glUniformMatrix4fv(gBuf.model, 1, GL_FALSE, glm::value_ptr(identity));
        glUniformMatrix3fv(gBuf.normalMat, 1, GL_FALSE, glm::value_ptr(normalMatrix));

        // Lighting shader
        lighting.gPosition = glGetUniformLocation(shaders.lighting, "gPosition");
        lighting.gNormal   = glGetUniformLocation(shaders.lighting, "gNormal");
        lighting.gAlbedo   = glGetUniformLocation(shaders.lighting, "gAlbedo");
        lighting.cameraPos = glGetUniformLocation(shaders.lighting, "cameraPos");
        lighting.ambient   = glGetUniformLocation(shaders.lighting, "ambientMultiplier");

        glUseProgram(shaders.lighting);
        glUniform1i(lighting.gPosition, 0);
        glUniform1i(lighting.gNormal, 1);
        glUniform1i(lighting.gAlbedo, 2);
        glUniform1f(lighting.ambient, AMBIENT_MULTIPLIER);

        // upload SSAO kernel samples
        glUseProgram(shaders.ssao);
        for (int i = 0; i < (int)ssaoKernelCache.size(); i++)
        {
            std::string name = "samples[" + std::to_string(i) + "]";
            glUniform3fv(glGetUniformLocation(shaders.ssao, name.c_str()), 1,
                         glm::value_ptr(ssaoKernelCache[i]));
        }
        glUniform1i(glGetUniformLocation(shaders.ssao, "gPosition"), 0);
        glUniform1i(glGetUniformLocation(shaders.ssao, "gNormal"), 1);
        glUniform1i(glGetUniformLocation(shaders.ssao, "noiseTexture"), 2);

        ssaoKernelCache.clear();
    }

    void Renderer::RenderFrame(float deltaTime)
    {
        glm::mat4 view = camera.GetViewMatrix();

        ShadowPass();

        int shadowCount = 0;
        for (auto& obj : scene->objects)
        {
            auto lc = obj->GetComponent<LightComponent>();
            if (!lc || !lc->light->castsShadow || !obj->enabled) continue;
            if (shadowCount >= MAX_SHADOW_LIGHTS) break;
            shadowCount++;
        }

        GeometryPass(view);
        if (SSAO_ENABLED) SSAOPass(view);
        LightingPass(shadowCount);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, gbuffer.FBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rt.litFBO);
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, rt.litFBO);

        if (SSR_ENABLED) {
            SSRPass();
            SSRCompositePass();

            glBindFramebuffer(GL_READ_FRAMEBUFFER, rt.ssrCompositeFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rt.litFBO);
            glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }

        SkyboxPass();
        ParticlePass(shadowCount);
        WaterPass(deltaTime);

        std::shared_ptr<CloudVolumeComponent> cv = nullptr;
        for (auto& obj : scene->objects)
        {
            cv = obj->GetComponent<CloudVolumeComponent>();
            if (cv)
                break;
        }

        if (FOG_ENABLED) FogPass(shadowCount);
        else PassthroughPass();

        if (CLOUD_ENABLED && cv) {
            Light* sun = scene->GetMainLight();
            if (sun) {
                CloudLightingPass(*cv->volume, sun);
                CloudPass(*cv->volume, sun);
                CloudCompositePass();
            }
        }

        FXAAPass();
        RenderDebugTexture();
    }

    void Renderer::RenderDebugTexture()
    {
        if (!RENDER_DEBUG_TEXTURE) return;

        const GLuint textures[] = {
            rt.litTexture,
            rt.fogTexture,
            rt.ssaoTexture,
            rt.ssaoBlurTexture,
            rt.fxaaTexture,
            rt.ssrTexture,
            rt.ssrCompositeTexture,
            rt.cloudTexture,
            rt.cloudCompositeTexture,
        };
        constexpr int count = sizeof(textures) / sizeof(textures[0]);
        GLuint tex = textures[glm::clamp(DEBUG_TEXTURE_INDEX, 0, count - 1)];

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaders.passthrough);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(glGetUniformLocation(shaders.passthrough, "litScene"), 0);
        quad.Draw();
    }

    void Renderer::ShadowPass(bool staticOnly)
    {
        profiler.Begin("Shadow Pass");
        int idx = 0;
        glUseProgram(shaders.shadow);
        glDisable(GL_CULL_FACE);

        for (auto& obj : scene->objects)
        {
            auto lc = obj->GetComponent<LightComponent>();
            if (!lc || !lc->light->castsShadow) continue;
            if (staticOnly && !obj->isStatic) continue;
            if (idx >= MAX_SHADOW_LIGHTS) break;

            auto& light = lc->light;

            if (light->type == LightType::Point)
            {
                glUseProgram(shaders.pointShadow);
                glEnable(GL_DEPTH_TEST);

                float near = 0.1f;
                float far = light->radius;
                glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, near, far);
                glm::vec3 pos = obj->transform.position;

                glm::mat4 shadowTransforms[6] = {
                    shadowProj * glm::lookAt(pos, pos + glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
                    shadowProj * glm::lookAt(pos, pos + glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
                    shadowProj * glm::lookAt(pos, pos + glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
                    shadowProj * glm::lookAt(pos, pos + glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
                    shadowProj * glm::lookAt(pos, pos + glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
                    shadowProj * glm::lookAt(pos, pos + glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0)),
                };

                glUniform3fv(glGetUniformLocation(shaders.pointShadow, "lightPos"), 1, glm::value_ptr(pos));
                glUniform1f(glGetUniformLocation(shaders.pointShadow, "farPlane"), far);

                glViewport(0, 0, POINT_SHADOW_RESOLUTION, POINT_SHADOW_RESOLUTION);

                for (int face = 0; face < 6; face++)
                {
                    glBindFramebuffer(GL_FRAMEBUFFER, light->shadowCubeFBO);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                           GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                                           light->shadowCubemap, 0);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                           GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                                           light->shadowCubeDepthRBO, 0);

                    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
                    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
                    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

                    glUniformMatrix4fv(glGetUniformLocation(shaders.pointShadow, "lightSpaceMatrix"),
                                       1, GL_FALSE, glm::value_ptr(shadowTransforms[face]));

                    for (auto& renderObj : scene->objects)
                    {
                        auto mc = renderObj->GetComponent<MeshComponent>();
                        if (!mc || !renderObj->enabled) continue;
                        if (renderObj->IsForwardRendered()) continue;
                        glm::mat4 model = renderObj->transform.GetMatrix();
                        glUniformMatrix4fv(glGetUniformLocation(shaders.pointShadow, "model"),
                                           1, GL_FALSE, glm::value_ptr(model));
                        mc->model->DrawGeometry();
                    }
                }

                // Restore layered attachment for cubemap sampling
                glBindFramebuffer(GL_FRAMEBUFFER, light->shadowCubeFBO);
                glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, light->shadowCubemap, 0);
                glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, light->shadowCubeDepthRBO, 0);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
            else
            {
                // Existing directional/spot shadow code
                glUseProgram(shaders.shadow);
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
                RenderShadowMap(light->lightSpaceMatrix);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }

            idx++;
        }

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        profiler.End("Shadow Pass");
    }

    void Renderer::GeometryPass(const glm::mat4& view)
    {
        profiler.Begin("Geometry Pass");
        glViewport(0, 0, w, h);
        gbuffer.Bind();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glUseProgram(shaders.gBuffer);
        glUniformMatrix4fv(gBuf.view, 1, GL_FALSE, glm::value_ptr(view));
        glUniform3f(glGetUniformLocation(shaders.gBuffer, "viewPos"),
                camera.transform.position.x,
                camera.transform.position.y,
                camera.transform.position.z);
        scene->RenderGeometry(shaders.gBuffer, gBuf.model);
        gbuffer.Unbind();
        profiler.End("Geometry Pass");
    }

    void Renderer::SSAOPass(const glm::mat4& view)
    {
        // SSAO
        profiler.Begin("SSAO Pass");
        glBindFramebuffer(GL_FRAMEBUFFER, rt.ssaoFBO);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaders.ssao);

        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gbuffer.gPosition);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gbuffer.gNormal);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, rt.ssaoNoiseTex);

        glUniformMatrix4fv(glGetUniformLocation(shaders.ssao, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaders.ssao, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform2f(glGetUniformLocation(shaders.ssao, "screenSize"), (float)w, (float)h);
        glUniform1f(glGetUniformLocation(shaders.ssao, "radius"), SSAO_RADIUS);
        glUniform1f(glGetUniformLocation(shaders.ssao, "bias"), SSAO_BIAS);
        glUniform1i(glGetUniformLocation(shaders.ssao, "kernelSize"), SSAO_KERNEL_SIZE);

        quad.Draw();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        profiler.End("SSAO Pass");

        // Blur
        profiler.Begin("SSAO Blur");
        glBindFramebuffer(GL_FRAMEBUFFER, rt.ssaoBlurFBO);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaders.ssaoBlur);

        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, rt.ssaoTexture);
        glUniform1i(glGetUniformLocation(shaders.ssaoBlur, "ssaoInput"), 0);

        quad.Draw();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        profiler.End("SSAO Blur");
    }

    void Renderer::LightingPass(int shadowCount)
    {
        profiler.Begin("Lighting Pass");
        glBindFramebuffer(GL_FRAMEBUFFER, rt.litFBO);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaders.lighting);

        glUniform1f(lighting.ambient, AMBIENT_MULTIPLIER);

        glUniform1i(glGetUniformLocation(shaders.lighting, "pcfKernelSize"), PCF_KERNEL_SIZE);
        glUniform1i(glGetUniformLocation(shaders.lighting, "shadowLightCount"), shadowCount);
        glUniform1i(glGetUniformLocation(shaders.lighting, "pcfEnabled"), PCF_ENABLED ? 1 : 0);
        glUniform1f(glGetUniformLocation(shaders.lighting, "shadowBias"), SHADOW_BIAS);
        glUniform1f(glGetUniformLocation(shaders.lighting, "shadowNormalOffset"), SHADOW_NORMAL_OFFSET);
        glUniform1f(glGetUniformLocation(shaders.lighting, "pointShadowFarPlane"), POINT_SHADOW_FAR_PLANE);


        // GBUFFER locked slots 0-3
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gbuffer.gPosition);
        glUniform1i(glGetUniformLocation(shaders.lighting, "gPosition"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gbuffer.gNormal);
        glUniform1i(glGetUniformLocation(shaders.lighting, "gNormal"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gbuffer.gAlbedo);
        glUniform1i(glGetUniformLocation(shaders.lighting, "gAlbedo"), 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, gbuffer.gEmissive);
        glUniform1i(glGetUniformLocation(shaders.lighting, "gEmissive"), 3);


        // Shadow maps 4+
        int baseSlot = 4;

        int si = 0;
        for (auto& obj : scene->objects)
        {
            auto lc = obj->GetComponent<LightComponent>();
            if (!lc || !lc->light->castsShadow || !obj->enabled) continue;
            if (si >= MAX_SHADOW_LIGHTS) break;

            glActiveTexture(GL_TEXTURE0 + baseSlot + si);

            if (lc->light->type == LightType::Point)
                glBindTexture(GL_TEXTURE_2D, 0);
            else
                glBindTexture(GL_TEXTURE_2D, lc->light->shadowMap);

            std::string name = "shadowMap[" + std::to_string(si) + "]";
            glUniform1i(glGetUniformLocation(shaders.lighting, name.c_str()), baseSlot + si);

            std::string lsm = "lightSpaceMatrix[" + std::to_string(si) + "]";
            glUniformMatrix4fv(glGetUniformLocation(shaders.lighting, lsm.c_str()),
                               1, GL_FALSE, glm::value_ptr(lc->light->lightSpaceMatrix));

            si++;
        }

        // Cube maps after shadow maps
        int cubeStart = baseSlot + si;

        int ci = 0;
        for (auto& obj : scene->objects)
        {
            auto lc = obj->GetComponent<LightComponent>();
            if (!lc || !lc->light->castsShadow || !obj->enabled) continue;
            if (ci >= MAX_SHADOW_LIGHTS) break;

            glActiveTexture(GL_TEXTURE0 + cubeStart + ci);

            if (lc->light->type == LightType::Point)
                glBindTexture(GL_TEXTURE_CUBE_MAP, lc->light->shadowCubemap);
            else
                glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

            std::string name = "shadowCubeMap[" + std::to_string(ci) + "]";
            glUniform1i(glGetUniformLocation(shaders.lighting, name.c_str()), cubeStart + ci);

            ci++;
        }

        // Far planes after cube maps
        int fi = 0;
        for (auto& obj : scene->objects)
        {
            auto lc = obj->GetComponent<LightComponent>();
            if (!lc || !lc->light->castsShadow || !obj->enabled) continue;
            if (fi >= MAX_SHADOW_LIGHTS) break;

            float fp = (lc->light->type == LightType::Point) ? lc->light->radius : 50.0f;

            std::string name = "lightFarPlane[" + std::to_string(fi) + "]";
            glUniform1f(glGetUniformLocation(shaders.lighting, name.c_str()), fp);

            fi++;
        }


        // SSAO last slot

        int ssaoSlot = cubeStart + ci;

        glActiveTexture(GL_TEXTURE0 + ssaoSlot);
        glBindTexture(GL_TEXTURE_2D, SSAO_ENABLED ? rt.ssaoBlurTexture : 0);

        glUniform1i(glGetUniformLocation(shaders.lighting, "ssaoTexture"), ssaoSlot);
        glUniform1i(glGetUniformLocation(shaders.lighting, "ssaoEnabled"), SSAO_ENABLED ? 1 : 0);


        // Camera
        glUniform3f(lighting.cameraPos,
                    camera.transform.position.x,
                    camera.transform.position.y,
                    camera.transform.position.z);

        scene->UploadLights(shaders.lighting);

        glUniform1i(glGetUniformLocation(shaders.lighting, "giMode"), GI_MODE);
        glUniform1f(glGetUniformLocation(shaders.lighting, "giIntensity"), GI_INTENSITY);

        if (GI_MODE >= 1 && probeSSBO != 0) {
            auto& grid = scene->probeGrid;
            glUniform3fv(glGetUniformLocation(shaders.lighting, "probeGridMin"),
                         1, glm::value_ptr(grid.boundsMin));
            glUniform3fv(glGetUniformLocation(shaders.lighting, "probeGridMax"),
                         1, glm::value_ptr(grid.boundsMax));
            glUniform3i(glGetUniformLocation(shaders.lighting, "probeGridCount"),
                        grid.countX, grid.countY, grid.countZ);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, probeSSBO);
        }

        quad.Draw();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        profiler.End("Lighting Pass");
    }

    void Renderer::FogPass(int shadowCount)
    {
        profiler.Begin("Fog Pass");
        glDisable(GL_DEPTH_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, rt.fogFBO);
        glViewport(0, 0, w / FOG_RESOLUTION_SCALE, h / FOG_RESOLUTION_SCALE);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaders.fog);

        scene->UploadFogVolumes(shaders.fog);

        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, rt.litTexture);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gbuffer.gPosition);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, rt.fogNoiseTexture);

        glUniform1i(glGetUniformLocation(shaders.fog, "litScene"), 0);
        glUniform1i(glGetUniformLocation(shaders.fog, "gPosition"), 1);
        glUniform1i(glGetUniformLocation(shaders.fog, "noiseTexture"), 2);
        glUniform1f(glGetUniformLocation(shaders.fog, "fogScale"), FOG_SCALE);
        glUniform1f(glGetUniformLocation(shaders.fog, "fogScrollSpeed"), FOG_SCROLL_SPEED);

        int si = 0;
        for (auto& obj : scene->objects)
        {
            auto lc = obj->GetComponent<LightComponent>();
            if (!lc || !lc->light->castsShadow || !obj->enabled) continue;
            if (si >= MAX_SHADOW_LIGHTS) break;

            glActiveTexture(GL_TEXTURE3 + si);
            if (lc->light->type == LightType::Point)
                glBindTexture(GL_TEXTURE_2D, 0);
            else
                glBindTexture(GL_TEXTURE_2D, lc->light->shadowMap);

            std::string name = "shadowMap[" + std::to_string(si) + "]";
            glUniform1i(glGetUniformLocation(shaders.fog, name.c_str()), 3 + si);
            std::string lsm = "lightSpaceMatrix[" + std::to_string(si) + "]";
            glUniformMatrix4fv(glGetUniformLocation(shaders.fog, lsm.c_str()),
                               1, GL_FALSE, glm::value_ptr(lc->light->lightSpaceMatrix));
            si++;
        }

        int cubeStart = 3 + si;
        int ci = 0;
        for (auto& obj : scene->objects)
        {
            auto lc = obj->GetComponent<LightComponent>();
            if (!lc || !lc->light->castsShadow || !obj->enabled) continue;
            if (ci >= MAX_SHADOW_LIGHTS) break;

            glActiveTexture(GL_TEXTURE0 + cubeStart + ci);
            if (lc->light->type == LightType::Point)
                glBindTexture(GL_TEXTURE_CUBE_MAP, lc->light->shadowCubemap);
            else
                glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

            std::string name = "shadowCubeMap[" + std::to_string(ci) + "]";
            glUniform1i(glGetUniformLocation(shaders.fog, name.c_str()), cubeStart + ci);
            ci++;
        }

        int fi = 0;
        for (auto& obj : scene->objects)
        {
            auto lc = obj->GetComponent<LightComponent>();
            if (!lc || !lc->light->castsShadow || !obj->enabled) continue;
            if (fi >= MAX_SHADOW_LIGHTS) break;

            float fp = (lc->light->type == LightType::Point) ? lc->light->radius : 50.0f;
            std::string name = "lightFarPlane[" + std::to_string(fi) + "]";
            glUniform1f(glGetUniformLocation(shaders.fog, name.c_str()), fp);
            fi++;
        }

        glUniform1f(glGetUniformLocation(shaders.fog, "pointShadowFarPlane"), POINT_SHADOW_FAR_PLANE);

        glUniform3f(glGetUniformLocation(shaders.fog, "cameraPos"),
                    camera.transform.position.x,
                    camera.transform.position.y,
                    camera.transform.position.z);
        glUniform1f(glGetUniformLocation(shaders.fog, "time"), (float)glfwGetTime());
        glUniform1f(glGetUniformLocation(shaders.fog, "fogDensity"), FOG_DENSITY);
        glUniform1i(glGetUniformLocation(shaders.fog, "steps"), FOG_STEPS);
        glUniform1i(glGetUniformLocation(shaders.fog, "shadowLightCount"), shadowCount);

        scene->UploadLights(shaders.fog);
        quad.Draw();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        profiler.End("Fog Pass");

        // Composite
        profiler.Begin("Fog Composite Pass");
        glBindFramebuffer(GL_FRAMEBUFFER, rt.fxaaFBO);
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaders.fogComposite);
        glBindFramebuffer(GL_FRAMEBUFFER, rt.fxaaFBO);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, rt.litTexture);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, rt.fogTexture);
        glUniform1i(glGetUniformLocation(shaders.fogComposite, "litScene"), 0);
        glUniform1i(glGetUniformLocation(shaders.fogComposite, "fogBuffer"), 1);
        glUniform2f(glGetUniformLocation(shaders.fogComposite, "resolution"), (float)w, (float)h);
        glUniform2f(glGetUniformLocation(shaders.fogComposite, "fogResolution"),
                    (float)(w / FOG_RESOLUTION_SCALE), (float)(h / FOG_RESOLUTION_SCALE));
        glUniform1i(glGetUniformLocation(shaders.fogComposite, "fogBlurKernelSize"), FOG_BLUR_KERNEL_SIZE);

        quad.Draw();
        glEnable(GL_DEPTH_TEST);
        glViewport(0, 0, w, h);
        profiler.End("Fog Composite Pass");
    }

    void Renderer::CloudPass(CloudVolume& vol, Light* sun) {
        profiler.Begin("Cloud Pass");

        glDisable(GL_DEPTH_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, rt.cloudFBO);
        glViewport(0, 0, w / CLOUD_RESOLUTION_SCALE, h / CLOUD_RESOLUTION_SCALE);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaders.cloud);

        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gbuffer.gPosition);
        glUniform1i(glGetUniformLocation(shaders.cloud, "gPosition"), 0);

        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_3D, vol.noiseTex);
        glUniform1i(glGetUniformLocation(shaders.cloud, "noiseTex"), 1);

        glUniform3fv(glGetUniformLocation(shaders.cloud, "cloudMin"), 1, glm::value_ptr(vol.min));
        glUniform3fv(glGetUniformLocation(shaders.cloud, "cloudMax"), 1, glm::value_ptr(vol.max));
        glUniform1f(glGetUniformLocation(shaders.cloud, "cloudScale"), vol.scale);
        glUniform1f(glGetUniformLocation(shaders.cloud, "cloudScrollSpeed"), vol.scrollSpeed);

        glUniform3fv(glGetUniformLocation(shaders.cloud, "cameraPos"), 1, glm::value_ptr(camera.transform.position));
        glUniform1f(glGetUniformLocation(shaders.cloud, "time"), (float)glfwGetTime());
        glUniform1i(glGetUniformLocation(shaders.cloud, "cloudSteps"), CLOUD_RAYMARCH_STEPS);
        glUniformMatrix4fv(glGetUniformLocation(shaders.cloud, "invViewProj"), 1, GL_FALSE,
            glm::value_ptr(glm::inverse(projection * camera.GetViewMatrix())));

        glUniform1f(glGetUniformLocation(shaders.cloud, "cloudAbsorption"), CLOUD_ABSORPTION);
        glUniform3fv(glGetUniformLocation(shaders.cloud, "lightDir"), 1, glm::value_ptr(sun->direction));
        glUniform3fv(glGetUniformLocation(shaders.cloud, "lightColor"), 1, glm::value_ptr(sun->color));

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_3D, vol.voxelLightTexture);
        glUniform1i(glGetUniformLocation(shaders.cloud, "voxelLightTex"), 2);
        std::cout << "CloudPass vol.min: " << vol.min.x << " " << vol.min.y << " " << vol.min.z << std::endl;
        std::cout << "CloudPass vol.max: " << vol.max.x << " " << vol.max.y << " " << vol.max.z << std::endl;
        std::cout << "CloudPass noiseTex: " << vol.noiseTex << " voxelLightTex: " << vol.voxelLightTexture << std::endl;

        quad.Draw();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, w, h);
        profiler.End("Cloud Pass");
    }

    void Renderer::CloudCompositePass() {
        profiler.Begin("Cloud Composite Pass");
        glBindFramebuffer(GL_FRAMEBUFFER, rt.cloudCompositeFBO);
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaders.cloudComposite);

        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, rt.fxaaTexture);
        glUniform1i(glGetUniformLocation(shaders.cloudComposite, "litScene"), 0);

        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, rt.cloudTexture);
        glUniform1i(glGetUniformLocation(shaders.cloudComposite, "cloudBuffer"), 1);

        quad.Draw();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        profiler.End("Cloud Composite Pass");
    }

    void Renderer::CloudLightingPass(CloudVolume& vol, Light* sun) {
        if (!sun) return;

        if (vol.voxelLightTexture == 0) {
            vol.InitializeVoxelGrid();
        }

        cloudLightingFrameCounter++;
        if (cloudLightingFrameCounter % CLOUD_LIGHTING_UPDATE_INTERVAL != 0) return;

        profiler.Begin("Cloud Lighting Pass");

        glUseProgram(shaders.cloudLighting);

        glBindImageTexture(0, vol.voxelLightTexture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        glUniform1i(glGetUniformLocation(shaders.cloudLighting, "voxelOutput"), 0);

        glUniform3fv(glGetUniformLocation(shaders.cloudLighting, "cloudMin"), 1, glm::value_ptr(vol.min));
        glUniform3fv(glGetUniformLocation(shaders.cloudLighting, "cloudMax"), 1, glm::value_ptr(vol.max));
        glUniform3i(glGetUniformLocation(shaders.cloudLighting, "voxelResolution"),
            vol.lightingVoxelGridSize.x, vol.lightingVoxelGridSize.y, vol.lightingVoxelGridSize.z);

        glUniform1f(glGetUniformLocation(shaders.cloudLighting, "cloudScale"), vol.scale);
        glUniform1f(glGetUniformLocation(shaders.cloudLighting, "cloudScrollSpeed"), vol.scrollSpeed);
        glUniform1f(glGetUniformLocation(shaders.cloudLighting, "time"), (float)glfwGetTime());

        // RGBA Weights
        glUniform1f(glGetUniformLocation(shaders.cloudLighting, "rWeight"), vol.rWeight);
        glUniform1f(glGetUniformLocation(shaders.cloudLighting, "gWeight"), vol.gWeight);
        glUniform1f(glGetUniformLocation(shaders.cloudLighting, "bWeight"), vol.bWeight);
        glUniform1f(glGetUniformLocation(shaders.cloudLighting, "aWeight"), vol.aWeight);

        // Lighting settings
        glUniform1i(glGetUniformLocation(shaders.cloudLighting, "lightingSteps"), CLOUD_RAYMARCH_LIGHTING_STEPS);
        glUniform1f(glGetUniformLocation(shaders.cloudLighting, "lightingStepScale"), CLOUD_RAYMARCH_LIGHTING_RAY_DEPTH);
        glUniform1f(glGetUniformLocation(shaders.cloudLighting, "cloudAbsorption"), CLOUD_ABSORPTION);

        // Sun
        glUniform3fv(glGetUniformLocation(shaders.cloudLighting, "lightDir"), 1, glm::value_ptr(sun->direction));
        glUniform3fv(glGetUniformLocation(shaders.cloudLighting, "lightColor"), 1, glm::value_ptr(sun->color));
        glUniform1f(glGetUniformLocation(shaders.cloudLighting, "lightIntensity"), sun->intensity);

        // Noise texture for density sampling
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, vol.noiseTex);
        glUniform1i(glGetUniformLocation(shaders.cloudLighting, "cloudNoiseTex"), 0);

        glDispatchCompute(
        (vol.lightingVoxelGridSize.x + 7) / 8,
        (vol.lightingVoxelGridSize.y + 7) / 8,
        (vol.lightingVoxelGridSize.z + 7) / 8
        );

        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

        profiler.End("Cloud Lighting Pass");
    }


    void Renderer::PassthroughPass()
    {
        glViewport(0, 0, w, h);
        glBindFramebuffer(GL_FRAMEBUFFER, rt.fxaaFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaders.passthrough);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, rt.litTexture);
        glUniform1i(glGetUniformLocation(shaders.passthrough, "litScene"), 0);
        quad.Draw();
    }

    void Renderer::FXAAPass()
    {
        profiler.Begin("FXAA Pass");
        glViewport(0, 0, w, h);
        glDisable(GL_DEPTH_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (FXAA_ENABLED)
        {
            glUseProgram(shaders.fxaa);
            glActiveTexture(GL_TEXTURE0);
            GLuint fxaaInput = (CLOUD_ENABLED) ? rt.cloudCompositeTexture : rt.fxaaTexture;
            glBindTexture(GL_TEXTURE_2D, fxaaInput);
            glUniform1i(glGetUniformLocation(shaders.fxaa, "screenTexture"), 0);
            glUniform2f(glGetUniformLocation(shaders.fxaa, "resolution"), (float)w, (float)h);
            glUniform1f(glGetUniformLocation(shaders.fxaa, "edgeThreshold"), FXAA_EDGE_THRESHOLD);
            glUniform1f(glGetUniformLocation(shaders.fxaa, "edgeThresholdMin"), FXAA_EDGE_THRESHOLD_MIN);
        }
        else
        {
            glUseProgram(shaders.passthrough);
            glActiveTexture(GL_TEXTURE0);
            GLuint fxaaInput = (CLOUD_ENABLED) ? rt.cloudCompositeTexture : rt.fxaaTexture;
            glBindTexture(GL_TEXTURE_2D, fxaaInput);
            glUniform1i(glGetUniformLocation(shaders.passthrough, "litScene"), 0);
        }

        quad.Draw();
        glEnable(GL_DEPTH_TEST);
        profiler.End("FXAA Pass");
    }

    void Renderer::SSRPass()
    {
        profiler.Begin("SSR Pass");

        glBindFramebuffer(GL_FRAMEBUFFER, rt.ssrFBO);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaders.ssr);

        // gAlbedo
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gbuffer.gAlbedo);
        glUniform1i(glGetUniformLocation(shaders.ssr, "gAlbedo"), 0);
        // gNormal
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gbuffer.gNormal);
        glUniform1i(glGetUniformLocation(shaders.ssr, "gNormal"), 1);
        // gPosition
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gbuffer.gPosition);
        glUniform1i(glGetUniformLocation(shaders.ssr, "gPosition"), 2);
        // sceneColor (lit buffer)
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, rt.litTexture);
        glUniform1i(glGetUniformLocation(shaders.ssr, "sceneColor"), 3);

        glUniform3fv(glGetUniformLocation(shaders.ssr, "cameraPos"), 1, &camera.transform.position[0]);
        glUniformMatrix4fv(glGetUniformLocation(shaders.ssr, "view"), 1, GL_FALSE, &camera.GetViewMatrix()[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shaders.ssr, "projection"), 1, GL_FALSE, &projection[0][0]);
        glUniform1f(glGetUniformLocation(shaders.ssr, "minStepSize"), SSR_MIN_STEP_SIZE);
        glUniform1f(glGetUniformLocation(shaders.ssr, "maxStepSize"), SSR_MAX_STEP_SIZE);
        glUniform1i(glGetUniformLocation(shaders.ssr, "raymarchSteps"), SSR_RAYMARCH_STEPS);

        quad.Draw();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        profiler.End("SSR Pass");
    }

    void Renderer::SSRCompositePass() {
        profiler.Begin("SSR Composite Pass");

        glBindFramebuffer(GL_FRAMEBUFFER, rt.ssrCompositeFBO);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaders.ssrComposite);

        glUniform1i(glGetUniformLocation(shaders.ssrComposite, "ssrEnabled"), SSR_ENABLED ? 1 : 0);
        glUniform1i(glGetUniformLocation(shaders.ssrComposite, "probeEnabled"), REFLECTION_PROBE_ENABLED ? 1 : 0);

        // Upload all needed textures from gbuffer
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, rt.litTexture);
        glUniform1i(glGetUniformLocation(shaders.ssrComposite, "litScene"), 0);

        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, rt.ssrTexture);
        glUniform1i(glGetUniformLocation(shaders.ssrComposite, "ssrTexture"), 1);

        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, gbuffer.gNormal);
        glUniform1i(glGetUniformLocation(shaders.ssrComposite, "gNormal"), 2);

        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, gbuffer.gAlbedo);
        glUniform1i(glGetUniformLocation(shaders.ssrComposite, "gAlbedo"), 3);

        glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, gbuffer.gPosition);
        glUniform1i(glGetUniformLocation(shaders.ssrComposite, "gPosition"), 4);

        glUniform3fv(glGetUniformLocation(shaders.ssrComposite, "cameraPos"), 1, &camera.transform.position[0]);

        // Upload Reflection probe textures at slot 5+
        int probeCount = REFLECTION_PROBE_ENABLED
            ? (int)std::min(reflectionProbes.size(), (size_t)MAX_REFLECTION_PROBES)
            : 0;

        glUniform1i(glGetUniformLocation(shaders.ssrComposite, "probeCount"), probeCount);

        for (int i = 0; i < probeCount; i++) {
            auto& [probe, pos] = reflectionProbes[i];

            glm::vec3 worldMin = probe->boundsMin + pos;
            glm::vec3 worldMax = probe->boundsMax + pos;

            glActiveTexture(GL_TEXTURE5 + i);
            if (probe->baked)
                glBindTexture(GL_TEXTURE_CUBE_MAP, probe->cubemap);
            else
                glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

            std::string slot = "reflectionProbes[" + std::to_string(i) + "]";
            std::string bMin = "probeBoundsMin[" + std::to_string(i) + "]";
            std::string bMax = "probeBoundsMax[" + std::to_string(i) + "]";
            std::string posU = "probePositions[" + std::to_string(i) + "]";
            std::string br   = "probeBlendRadius[" + std::to_string(i) + "]";

            glUniform1i(glGetUniformLocation(shaders.ssrComposite, slot.c_str()), 5 + i);
            glUniform3fv(glGetUniformLocation(shaders.ssrComposite, bMin.c_str()), 1, glm::value_ptr(worldMin));
            glUniform3fv(glGetUniformLocation(shaders.ssrComposite, bMax.c_str()), 1, glm::value_ptr(worldMax));
            glUniform3fv(glGetUniformLocation(shaders.ssrComposite, posU.c_str()), 1, glm::value_ptr(pos));
            glUniform1f(glGetUniformLocation(shaders.ssrComposite,  br.c_str()),   probe->blendRadius);
        }

        if (scene->skybox) {
            int skyboxSlot = 5 + probeCount;
            glActiveTexture(GL_TEXTURE0 + skyboxSlot);
            glBindTexture(GL_TEXTURE_CUBE_MAP, scene->skybox->GetCubemapID());
            glUniform1i(glGetUniformLocation(shaders.ssrComposite, "skybox"), skyboxSlot);
            glUniform1i(glGetUniformLocation(shaders.ssrComposite, "hasSkybox"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shaders.ssrComposite, "hasSkybox"), 0);
        }

        quad.Draw();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        profiler.End("SSR Composite Pass");
    }

    void Renderer::ParticlePass(int shadowCount)
    {
        profiler.Begin("Particle Pass");
        glBindFramebuffer(GL_FRAMEBUFFER, rt.litFBO);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, gbuffer.FBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rt.litFBO);
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, rt.litFBO);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive
        glEnable(GL_PROGRAM_POINT_SIZE);
        glEnable(0x8861); // GL_POINT_SPRITE constant removed from core headers

        glm::mat4 view = camera.GetViewMatrix();

        for (auto& obj: scene->objects)
        {
            if (!obj->enabled) continue;

            auto ps = obj->GetComponent<ParticleSystemComponent>();
            if (!ps) continue;

            unsigned int shader = ps->system->IsLit() ? shaders.particleLit : shaders.particleUnlit;
            glUseProgram(shader);

            glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniform3f(glGetUniformLocation(shader, "cameraPos"),
                        camera.transform.position.x, camera.transform.position.y, camera.transform.position.z);

            if (ps->system->IsLit())
            {
                int shadowIndex = 0;
                for (auto& shadowObj : scene->objects) {
                    auto lc = shadowObj->GetComponent<LightComponent>();
                    if (!lc || !shadowObj->enabled || !lc->light->castsShadow)
                        continue;
                    if (shadowIndex >= MAX_SHADOW_LIGHTS) break;

                    glActiveTexture(GL_TEXTURE0 + shadowIndex);
                    if (lc->light->type == LightType::Point)
                        glBindTexture(GL_TEXTURE_2D, 0);
                    else
                        glBindTexture(GL_TEXTURE_2D, lc->light->shadowMap);

                    std::string name = "shadowMap[" + std::to_string(shadowIndex) + "]";
                    glUniform1i(glGetUniformLocation(shader, name.c_str()), shadowIndex);
                    std::string lsm = "lightSpaceMatrix[" + std::to_string(shadowIndex) + "]";
                    glUniformMatrix4fv(glGetUniformLocation(shader, lsm.c_str()),
                                       1, GL_FALSE, glm::value_ptr(lc->light->lightSpaceMatrix));
                    shadowIndex++;
                }

                int cubeStart = shadowIndex;
                int ci = 0;
                for (auto& shadowObj : scene->objects) {
                    auto lc = shadowObj->GetComponent<LightComponent>();
                    if (!lc || !shadowObj->enabled || !lc->light->castsShadow)
                        continue;
                    if (ci >= MAX_SHADOW_LIGHTS) break;

                    glActiveTexture(GL_TEXTURE0 + cubeStart + ci);
                    if (lc->light->type == LightType::Point)
                        glBindTexture(GL_TEXTURE_CUBE_MAP, lc->light->shadowCubemap);
                    else
                        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

                    std::string name = "shadowCubeMap[" + std::to_string(ci) + "]";
                    glUniform1i(glGetUniformLocation(shader, name.c_str()), cubeStart + ci);
                    ci++;
                }

                int fi = 0;
                for (auto& shadowObj : scene->objects)
                {
                    auto lc = shadowObj->GetComponent<LightComponent>();
                    if (!lc || !lc->light->castsShadow) continue;
                    if (fi >= MAX_SHADOW_LIGHTS) break;

                    float fp = (lc->light->type == LightType::Point) ? lc->light->radius : 50.0f;
                    std::string name = "lightFarPlane[" + std::to_string(fi) + "]";
                    glUniform1f(glGetUniformLocation(shader, name.c_str()), fp);
                    fi++;
                }

                glUniform1f(glGetUniformLocation(shader, "pointShadowFarPlane"), POINT_SHADOW_FAR_PLANE);
                glUniform1i(glGetUniformLocation(shader, "shadowLightCount"), shadowCount);
                scene->UploadLights(shader);
            }

            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ps->system->GetSSBO());
            glBindVertexArray(ps->system->GetDummyVAO());
            glDrawArrays(GL_POINTS, 0, ps->system->GetCount());
        }

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glBindVertexArray(0);

        profiler.End("Particle Pass");
    }

    void Renderer::SkyboxPass()
    {
        if (!scene->skybox || !SKYBOX_ENABLED) return;

        profiler.Begin("Skybox Pass");

        glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE);
        glUseProgram(shaders.skybox);

        glm::mat4 view = glm::mat4(glm::mat3(camera.GetViewMatrix()));

        glUniformMatrix4fv(glGetUniformLocation(shaders.skybox, "view"),
                           1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaders.skybox, "projection"),
                           1, GL_FALSE, glm::value_ptr(projection));
        glUniform1i(glGetUniformLocation(shaders.skybox, "skybox"), 0);

        scene->skybox->Draw();

        // Unbind the cubemap so it doesn't interfere with subsequent 2D texture binds
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        glEnable(GL_CULL_FACE);
        glDepthFunc(GL_LESS);

        profiler.End("Skybox Pass");
    }

    void Renderer::WaterPass(float deltaTime) {
        profiler.Begin("Water Pass");
        glBindFramebuffer(GL_FRAMEBUFFER, rt.litFBO);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        glm::mat4 view = camera.GetViewMatrix();

        for (auto& obj: scene->objects) {
            if (!obj->enabled) continue;
            auto wc = obj->GetComponent<InteractiveWaterComponent>();
            if (!wc) continue;

            glm::vec3 playerPos = camera.transform.position;
            glm::vec3 waterPos  = obj->transform.position;
            float waterSize     = obj->transform.scale.x;

            // Simulate
            wc->interactiveWater->Update(deltaTime);
            if (abs(playerPos.y - waterPos.y) < 1.5f)
                wc->interactiveWater->AddRippleWorld(playerPos, waterPos, waterSize);
            wc->interactiveWater->ComputeHeightMap();
            wc->interactiveWater->ComputeNormals();

            unsigned int shader = wc->GetWaterShader();
            glUseProgram(shader);

            // Matrices
            glUniformMatrix4fv(glGetUniformLocation(shader, "model"),
                1, GL_FALSE, glm::value_ptr(obj->transform.GetMatrix()));
            glUniformMatrix4fv(glGetUniformLocation(shader, "view"),
                1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(shader, "projection"),
                1, GL_FALSE, glm::value_ptr(projection));

            // Water params
            glUniform1f(glGetUniformLocation(shader, "rippleStrength"),
                wc->interactiveWater->rippleStrength);
            glUniform3fv(glGetUniformLocation(shader, "cameraPos"), 1,
                glm::value_ptr(camera.transform.position));
            glUniform3fv(glGetUniformLocation(shader, "shallowColor"), 1,
                glm::value_ptr(wc->interactiveWater->shallowColor));
            glUniform3fv(glGetUniformLocation(shader, "deepColor"), 1,
                glm::value_ptr(wc->interactiveWater->deepColor));
            glUniform1f(glGetUniformLocation(shader, "fresnelPower"),
                wc->interactiveWater->fresnelPower);
            glUniform1f(glGetUniformLocation(shader, "specularStrength"),
                wc->interactiveWater->specularStrength);

            // Slot 0: height map
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, wc->interactiveWater->GetHeightMap());
            glUniform1i(glGetUniformLocation(shader, "heightMap"), 0);

            // Slot 1: normal map
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, wc->interactiveWater->GetNormalMap());
            glUniform1i(glGetUniformLocation(shader, "normalMap"), 1);

            // Scene lights
            scene->UploadLights(shader);

            // Slots 2+: shadow maps
            int si = 0;
            for (auto& shadowObj : scene->objects) {
                auto lc = shadowObj->GetComponent<LightComponent>();
                if (!lc || !lc->light->castsShadow || !shadowObj->enabled) continue;
                if (si >= MAX_SHADOW_LIGHTS) break;

                glActiveTexture(GL_TEXTURE2 + si);
                if (lc->light->type == LightType::Point)
                    glBindTexture(GL_TEXTURE_2D, 0);
                else
                    glBindTexture(GL_TEXTURE_2D, lc->light->shadowMap);

                glUniform1i(glGetUniformLocation(shader,
                    ("shadowMap[" + std::to_string(si) + "]").c_str()), 2 + si);
                glUniformMatrix4fv(glGetUniformLocation(shader,
                    ("lightSpaceMatrix[" + std::to_string(si) + "]").c_str()),
                    1, GL_FALSE, glm::value_ptr(lc->light->lightSpaceMatrix));
                si++;
            }

            // Cubemap shadows
            int cubeStart = 2 + si;
            int ci = 0;
            for (auto& shadowObj : scene->objects) {
                auto lc = shadowObj->GetComponent<LightComponent>();
                if (!lc || !lc->light->castsShadow || !shadowObj->enabled) continue;
                if (ci >= MAX_SHADOW_LIGHTS) break;

                glActiveTexture(GL_TEXTURE0 + cubeStart + ci);
                if (lc->light->type == LightType::Point)
                    glBindTexture(GL_TEXTURE_CUBE_MAP, lc->light->shadowCubemap);
                else
                    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

                glUniform1i(glGetUniformLocation(shader,
                    ("shadowCubeMap[" + std::to_string(ci) + "]").c_str()), cubeStart + ci);
                ci++;
            }

            // Far planes
            int fi = 0;
            for (auto& shadowObj : scene->objects) {
                auto lc = shadowObj->GetComponent<LightComponent>();
                if (!lc || !lc->light->castsShadow || !shadowObj->enabled) continue;
                if (fi >= MAX_SHADOW_LIGHTS) break;
                float fp = (lc->light->type == LightType::Point) ? lc->light->radius : 50.0f;
                glUniform1f(glGetUniformLocation(shader,
                    ("lightFarPlane[" + std::to_string(fi) + "]").c_str()), fp);
                fi++;
            }

            glUniform1i(glGetUniformLocation(shader, "shadowLightCount"), si);
            glUniform1f(glGetUniformLocation(shader, "pointShadowFarPlane"), POINT_SHADOW_FAR_PLANE);

            // Reflection probes
            int probeStart = cubeStart + ci;
            int probeCount = (int)std::min(reflectionProbes.size(), (size_t)MAX_REFLECTION_PROBES);
            glUniform1i(glGetUniformLocation(shader, "probeCount"), probeCount);
            for (int i = 0; i < probeCount; i++) {
                auto& [probe, pos] = reflectionProbes[i];
                glm::vec3 worldMin = probe->boundsMin + pos;
                glm::vec3 worldMax = probe->boundsMax + pos;

                glActiveTexture(GL_TEXTURE0 + probeStart + i);
                glBindTexture(GL_TEXTURE_CUBE_MAP, probe->baked ? probe->cubemap : 0);
                glUniform1i(glGetUniformLocation(shader,
                    ("reflectionProbes[" + std::to_string(i) + "]").c_str()), probeStart + i);
                glUniform3fv(glGetUniformLocation(shader,
                    ("probeBoundsMin[" + std::to_string(i) + "]").c_str()), 1, glm::value_ptr(worldMin));
                glUniform3fv(glGetUniformLocation(shader,
                    ("probeBoundsMax[" + std::to_string(i) + "]").c_str()), 1, glm::value_ptr(worldMax));
                glUniform3fv(glGetUniformLocation(shader,
                    ("probePositions[" + std::to_string(i) + "]").c_str()), 1, glm::value_ptr(pos));
                glUniform1f(glGetUniformLocation(shader,
                    ("probeBlendRadius[" + std::to_string(i) + "]").c_str()), probe->blendRadius);
            }

            // Skybox
            int skyboxSlot = probeStart + probeCount;
            if (scene->skybox) {
                glActiveTexture(GL_TEXTURE0 + skyboxSlot);
                glBindTexture(GL_TEXTURE_CUBE_MAP, scene->skybox->GetCubemapID());
                glUniform1i(glGetUniformLocation(shader, "skybox"), skyboxSlot);
                glUniform1i(glGetUniformLocation(shader, "hasSkybox"), 1);
            } else {
                glUniform1i(glGetUniformLocation(shader, "hasSkybox"), 0);
            }

            wc->interactiveWater->DrawPlane();
        }

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        profiler.End("Water Pass");
    }

    void Renderer::RenderShadowMap(const glm::mat4& lightSpaceMatrix)
    {
        glUseProgram(shaders.shadow);
        glUniformMatrix4fv(glGetUniformLocation(shaders.shadow, "lightSpaceMatrix"),
                           1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

        for (auto& obj : scene->objects)
        {
            auto mc = obj->GetComponent<MeshComponent>();
            if (!mc || !obj->enabled) continue;
            if (obj->IsForwardRendered()) continue;
            glm::mat4 model = obj->transform.GetMatrix();
            glUniformMatrix4fv(glGetUniformLocation(shaders.shadow, "model"),
                               1, GL_FALSE, glm::value_ptr(model));
            mc->model->DrawGeometry();
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

    // Mode: 0=Position, 1=Normals, 2=Albedo, 3=SSAO, 4=SSAO Blur,
    //       5=Lighting, 6-8=Shadow Maps, 9=Fog

    unsigned int Renderer::GetDebugTexture(int mode, std::shared_ptr<Scene> scene) const
    {
        switch (mode)
        {
            case 0: return gbuffer.gPosition;
            case 1: return gbuffer.gNormal;
            case 2: return gbuffer.gAlbedo;
            case 3: return rt.ssaoTexture;
            case 4: return rt.ssaoBlurTexture;
            case 5: return rt.litTexture;
            case 6:
            case 7:
            case 8:
            {
                int idx = 0;
                for (auto& obj : scene->objects)
                {
                    auto lc = obj->GetComponent<LightComponent>();
                    if (!lc || !lc->light->castsShadow) continue;
                    if (idx == mode - 6) return lc->light->shadowMap;
                    idx++;
                }
                return 0;
            }
            case 9: return rt.fogTexture;
            default: return 0;
        }
    }

    unsigned int Renderer::PathTraceCubemapFromPoint(const glm::vec3& position,
                                                      int faceSize)
    {
        // Build PT scene once per bake session
        if (!ptSceneBuilt) {
            ptScene = BuildPTScene(scene);
            ptSceneBuilt = true;
        }

        int triOffset = 0;
        for (auto& obj : scene->objects) {
            if (!obj->enabled || !obj->isStatic) continue;
            auto mc = obj->GetComponent<MeshComponent>();
            if (!mc || !mc->model) continue;
            if (obj->IsForwardRendered()) continue;

            int objTris = 0;
            for (auto& sm : mc->model->subMeshes)
                objTris += (int)sm.indices.size() / 3;

            printf("Object '%s': tris %d-%d texIdx=%d\n",
                   obj->name.c_str(), triOffset, triOffset + objTris - 1,
                   triOffset < (int)ptScene.triTextureIndex.size() ? ptScene.triTextureIndex[triOffset] : -99);
            triOffset += objTris;
        }

        std::vector<PTLight> lights = ExtractLights(scene);

        // Cubemap face directions
        const glm::vec3 faceDirs[6] = {
            glm::vec3( 1, 0, 0), glm::vec3(-1, 0, 0),
            glm::vec3( 0, 1, 0), glm::vec3( 0,-1, 0),
            glm::vec3( 0, 0, 1), glm::vec3( 0, 0,-1),
        };
        const glm::vec3 faceUps[6] = {
            glm::vec3(0,-1, 0), glm::vec3(0,-1, 0),
            glm::vec3(0, 0, 1), glm::vec3(0, 0,-1),
            glm::vec3(0,-1, 0), glm::vec3(0,-1, 0),
        };

        // Allocate pixel data for all 6 faces
        std::vector<float> facePixels(faceSize * faceSize * 3 * 6, 0.0f);

        struct FaceBasis { glm::vec3 forward, right, up; };
        FaceBasis basis[6];
        for (int face = 0; face < 6; face++) {
            basis[face].forward = faceDirs[face];
            basis[face].up      = faceUps[face];
            basis[face].right   = glm::normalize(glm::cross(faceDirs[face], faceUps[face]));
            basis[face].up      = glm::normalize(glm::cross(basis[face].right, faceDirs[face]));
        }

        // Multi thread Path Tracing
        #pragma omp parallel for schedule(dynamic)
        for (int i = 0; i < 6 * faceSize * faceSize; i++)
        {
            int face = i / (faceSize * faceSize);
            int px   = i % (faceSize * faceSize);
            int x    = px % faceSize;
            int y    = px / faceSize;

            std::mt19937 localRng(42 + i);

            float u = (2.0f * (x + 0.5f) / faceSize) - 1.0f;
            float v = (2.0f * (y + 0.5f) / faceSize) - 1.0f;

            glm::vec3 dir = glm::normalize(basis[face].forward + basis[face].right * u + basis[face].up * v);

            glm::vec3 radiance = PathTraceProbe(
                ptScene, lights, position, dir,
                PATH_TRACING_GI_SAMPLES,
                PATH_TRACING_GI_BOUNCES,
                localRng
            );

            int idx = i * 3;
            facePixels[idx + 0] = radiance.r;
            facePixels[idx + 1] = radiance.g;
            facePixels[idx + 2] = radiance.b;
        }

        // Upload to GL cubemap so BakeLightProbes can read it back with glGetTexImage
        unsigned int cubeTex;
        glGenTextures(1, &cubeTex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTex);
        for (int face = 0; face < 6; face++) {
            float* faceData = facePixels.data() + face * faceSize * faceSize * 3;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB16F,
                         faceSize, faceSize, 0, GL_RGB, GL_FLOAT, faceData);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        return cubeTex;
    }

    unsigned int Renderer::RenderCubemapFromPoint(const glm::vec3& position,
                                                   int faceSize)
    {
        unsigned int cubeFBO, cubeTex, cubeDepth;

        glGenFramebuffers(1, &cubeFBO);
        glGenTextures(1, &cubeTex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTex);
        for (int i = 0; i < 6; i++) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                         faceSize, faceSize, 0, GL_RGB, GL_FLOAT, nullptr);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glGenRenderbuffers(1, &cubeDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, cubeDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, faceSize, faceSize);

        glm::mat4 captureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.01f, 100.0f);

        glm::mat4 captureViews[6] = {
            glm::lookAt(position, position + glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
            glm::lookAt(position, position + glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
            glm::lookAt(position, position + glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
            glm::lookAt(position, position + glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
            glm::lookAt(position, position + glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
            glm::lookAt(position, position + glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0)),
        };

        GLint prevViewport[4];
        glGetIntegerv(GL_VIEWPORT, prevViewport);
        glViewport(0, 0, faceSize, faceSize);

        int bake_modelLoc     = glGetUniformLocation(shaders.probeBake, "model");
        int bake_viewLoc      = glGetUniformLocation(shaders.probeBake, "view");
        int bake_projLoc      = glGetUniformLocation(shaders.probeBake, "projection");
        int bake_normalMatLoc = glGetUniformLocation(shaders.probeBake, "normalMatrix");
        int bake_cameraPosLoc = glGetUniformLocation(shaders.probeBake, "cameraPos");
        int bake_ambientLoc   = glGetUniformLocation(shaders.probeBake, "ambientMultiplier");
        int bake_roughnessLoc = glGetUniformLocation(shaders.probeBake, "roughness");
        int bake_metallicLoc  = glGetUniformLocation(shaders.probeBake, "metallic");
        int bake_diffuseTexLoc = glGetUniformLocation(shaders.probeBake, "diffuseTex");
        int bake_shadowCountLoc = glGetUniformLocation(shaders.probeBake, "shadowLightCount");

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);


        for (int face = 0; face < 6; face++) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            glBindTexture(GL_TEXTURE_2D, 0);

            glBindFramebuffer(GL_FRAMEBUFFER, cubeFBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, cubeTex, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                      GL_RENDERBUFFER, cubeDepth);

            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glBindFramebuffer(GL_FRAMEBUFFER, cubeFBO);

            // Render geometry with bake shader
            glUseProgram(shaders.probeBake);
            glUniformMatrix4fv(bake_projLoc, 1, GL_FALSE, glm::value_ptr(captureProj));
            glUniformMatrix4fv(bake_viewLoc, 1, GL_FALSE, glm::value_ptr(captureViews[face]));
            glUniform3fv(bake_cameraPosLoc, 1, glm::value_ptr(position));
            glUniform1f(bake_ambientLoc, AMBIENT_MULTIPLIER);
            glUniform1i(bake_diffuseTexLoc, 0);

            // Upload lights
            scene->UploadLights(shaders.probeBake, true);

            // Bind shadow maps (slot 1+)
            int shadowCount = 0;
            int si = 0;
            for (auto& obj : scene->objects) {
                auto lc = obj->GetComponent<LightComponent>();
                if (!lc || !lc->light->castsShadow || !obj->enabled) continue;
                if (!obj->isStatic) continue;
                if (si >= MAX_SHADOW_LIGHTS) break;

                glActiveTexture(GL_TEXTURE1 + si);
                if (lc->light->type == LightType::Point)
                    glBindTexture(GL_TEXTURE_2D, 0);
                else
                    glBindTexture(GL_TEXTURE_2D, lc->light->shadowMap);

                std::string name = "shadowMap[" + std::to_string(si) + "]";
                glUniform1i(glGetUniformLocation(shaders.probeBake, name.c_str()), 1 + si);
                std::string lsm = "lightSpaceMatrix[" + std::to_string(si) + "]";
                glUniformMatrix4fv(glGetUniformLocation(shaders.probeBake, lsm.c_str()),
                                   1, GL_FALSE, glm::value_ptr(lc->light->lightSpaceMatrix));
                si++;
                shadowCount++;
            }

            int cubeStart = 1 + si;
            int ci = 0;
            for (auto& obj : scene->objects) {
                auto lc = obj->GetComponent<LightComponent>();
                if (!lc || !lc->light->castsShadow || !obj->enabled) continue;
                if (!obj->isStatic) continue;
                if (ci >= MAX_SHADOW_LIGHTS) break;


                glActiveTexture(GL_TEXTURE0 + cubeStart + ci);
                if (lc->light->type == LightType::Point)
                    glBindTexture(GL_TEXTURE_CUBE_MAP, lc->light->shadowCubemap);
                else
                    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

                std::string name = "shadowCubeMap[" + std::to_string(ci) + "]";
                glUniform1i(glGetUniformLocation(shaders.probeBake, name.c_str()), cubeStart + ci);
                ci++;
            }

            int fi = 0;
            for (auto& obj : scene->objects) {
                auto lc = obj->GetComponent<LightComponent>();
                if (!lc || !lc->light->castsShadow || !obj->enabled) continue;
                if (!obj->isStatic) continue;
                if (fi >= MAX_SHADOW_LIGHTS) break;
                float fp = (lc->light->type == LightType::Point) ? lc->light->radius : 50.0f;
                std::string name = "lightFarPlane[" + std::to_string(fi) + "]";
                glUniform1f(glGetUniformLocation(shaders.probeBake, name.c_str()), fp);
                fi++;
            }
            glUniform1i(bake_shadowCountLoc, shadowCount);

            // Render each object using the bake shader
            for (auto& obj : scene->objects) {
                if (!obj->enabled) continue;
                auto mc = obj->GetComponent<MeshComponent>();
                if (!mc || !mc->model) continue;
                if (obj->IsForwardRendered()) continue;

                glm::mat4 modelMat = obj->transform.GetMatrix();
                static Material defaultMat = Model::DefaultMaterial();

                for (auto& sm : mc->model->subMeshes) {
                    glm::mat4 finalModel = modelMat * sm.nodeTransform;
                    glUniformMatrix4fv(bake_modelLoc, 1, GL_FALSE, glm::value_ptr(finalModel));

                    glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(finalModel)));
                    glUniformMatrix3fv(bake_normalMatLoc, 1, GL_FALSE, glm::value_ptr(normalMat));

                    Material& mat = (sm.materialIndex >= 0 &&
                                     sm.materialIndex < (int)mc->model->materials.size())
                        ? mc->model->materials[sm.materialIndex]
                        : defaultMat;

                    glUniform1f(bake_roughnessLoc, mat.roughness);
                    glUniform1f(bake_metallicLoc, mat.metallic);

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, mat.diffuseTexture);

                    glBindVertexArray(sm.VAO);
                    glDrawElements(GL_TRIANGLES, sm.indexCount, GL_UNSIGNED_INT, 0);
                }
            }
            glBindVertexArray(0);

            // Render skybox into this face
            if (SKYBOX_ENABLED && scene->skybox) {
                glDepthFunc(GL_LEQUAL);
                glDisable(GL_CULL_FACE);
                glUseProgram(shaders.skybox);

                glm::mat4 skyView = glm::mat4(glm::mat3(captureViews[face]));
                glUniformMatrix4fv(glGetUniformLocation(shaders.skybox, "view"),
                                   1, GL_FALSE, glm::value_ptr(skyView));
                glUniformMatrix4fv(glGetUniformLocation(shaders.skybox, "projection"),
                                   1, GL_FALSE, glm::value_ptr(captureProj));
                glUniform1i(glGetUniformLocation(shaders.skybox, "skybox"), 0);

                scene->skybox->Draw();

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

                glEnable(GL_CULL_FACE);
                glDepthFunc(GL_LESS);
            }
        }

        // Restore state
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
        glClearColor(0, 0, 0, 1);

        glDeleteRenderbuffers(1, &cubeDepth);
        glDeleteFramebuffers(1, &cubeFBO);

        return cubeTex;
    }

    void Renderer::BakeLightProbes() {

        auto& grid = scene->probeGrid;
        if (grid.probes.empty()) {
            std::cerr << "No probes to bake — call Build() first" << std::endl;
            return;
        }

        ShadowPass(true);

        ptSceneBuilt = false;

        int faceSize = (GI_MODE == 2) ? PATH_TRACING_GI_FACE_SIZE : 64;
        int totalPixels = faceSize * faceSize * 6;
        std::vector<float> pixelData(totalPixels * 3);

        std::cout << "Baking " << grid.TotalProbes() << " light probes ("
                  << (GI_MODE == 2 ? "path traced" : "rasterized") << ")..." << std::endl;

        for (int i = 0; i < (int)grid.probes.size(); i++) {
            auto& probe = grid.probes[i];

            // Create cubemap at point
            unsigned int cubeTex = (GI_MODE == 2)
                ? PathTraceCubemapFromPoint(probe.position, faceSize)
                : RenderCubemapFromPoint(probe.position, faceSize);

            // Read back cubemap
            glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTex);
            for (int face = 0; face < 6; face++) {
                glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0,
                              GL_RGB, GL_FLOAT,
                              &pixelData[face * faceSize * faceSize * 3]);
            }

            // Encode into SH
            EncodeSH(pixelData.data(), faceSize, probe.shCoeffs);
            probe.baked = true;

            // Clean up cubemap
            glDeleteTextures(1, &cubeTex);

            if ((i + 1) % 10 == 0 || i == (int)grid.probes.size() - 1)
                std::cout << "  Baked " << (i + 1) << " / " << grid.TotalProbes() << std::endl;
        }

        UploadProbeData(grid);
        std::cout << "Light probe baking complete." << std::endl;
    }

    void Renderer::UploadProbeData(const ProbeGrid& grid) {
        std::vector<glm::vec4> shData(grid.TotalProbes() * 9);
        for (int p = 0; p < grid.TotalProbes(); p++) {
            for (int i = 0; i < 9; i++) {
                shData[p * 9 + i] = glm::vec4(grid.probes[p].shCoeffs[i], 0);
            }
        }

        if (probeSSBO == 0)
            glGenBuffers(1, &probeSSBO);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, probeSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, shData.size() * sizeof(glm::vec4),
                     shData.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    void Renderer::EncodeSH(const float* cubemapData, int faceSize, glm::vec3 outSH[9]) {
        for (int i = 0; i < 9; i++) {
            outSH[i] = glm::vec3(0);
        }

        float weightSum = 0;
        // Loop over each texel in the cubemap
        for (int face = 0; face < 6; face++) {
            for (int y = 0; y < faceSize; y++) {
                for (int x = 0; x < faceSize; x++) {
                    // Get UV coordinates for texel
                    float u = (2.0f * (x + 0.5f) / faceSize) - 1.0f;
                    float v = (2.0f * (y + 0.5f) / faceSize) - 1.0f;
                    // Get face direction and normalize
                    glm::vec3 dir;
                    switch (face) {
                        case 0: dir = glm::vec3( 1, -v, -u); break;
                        case 1: dir = glm::vec3(-1, -v,  u); break;
                        case 2: dir = glm::vec3( u,  1,  v); break;
                        case 3: dir = glm::vec3( u, -1, -v); break;
                        case 4: dir = glm::vec3( u, -v,  1); break;
                        case 5: dir = glm::vec3(-u, -v, -1); break;
                    }
                    dir = glm::normalize(dir);
                    // Get solid angle, projecting cube map texel onto sphere to account for cube to sphere distortion
                    // temp = 1 + u^2 + v^2
                    // solid angle = 4 / (temp * sqrt(temp))
                    float temp = 1.0f + u * u + v * v;
                    float weight = 4.0f / (sqrt(temp) * temp);
                    weightSum += weight;
                    // read pixel rgb from flattened texel array
                    int idx = (face * faceSize * faceSize + y * faceSize + x) * 3;
                    glm::vec3 color(cubemapData[idx], cubemapData[idx+1], cubemapData[idx+2]);
                    // ???????? Voodoo fuckery ??????????????
                    // 9 coefficient spherical harmonic (l = 2)
                    float basis[9];
                    basis[0] = 0.282095f;                                     // l=0, 1
                    basis[1] = 0.488603f * dir.y;                             // l=1, 2-4
                    basis[2] = 0.488603f * dir.z;
                    basis[3] = 0.488603f * dir.x;
                    basis[4] = 1.092548f * dir.x * dir.y;                     // l=2, 5-9
                    basis[5] = 1.092548f * dir.y * dir.z;
                    basis[6] = 0.315392f * (3.0f * dir.z * dir.z - 1.0f);
                    basis[7] = 1.092548f * dir.x * dir.z;
                    basis[8] = 0.546274f * (dir.x * dir.x - dir.y * dir.y);

                    for (int i = 0; i < 9; i++) {
                        outSH[i] += color * basis[i] * weight;
                    }
                }
            }
        }
        float norm = (4.0f * 3.14159265f) / weightSum;
        for (int i = 0; i < 9; i++)
            outSH[i] *= norm;
    }

    void Renderer::BakeReflectionProbes() {
        if (!REFLECTION_PROBE_ENABLED) return;

        CollectReflectionProbes();
        ShadowPass(true);

        for (auto& [probe, pos] : reflectionProbes) {
            std::cout << "Baking probe at: " << pos.x << ", " << pos.y << ", " << pos.z << std::endl;
            std::cout << "Bounds world min: " << (probe->boundsMin + pos).x << ", "
                      << (probe->boundsMin + pos).y << ", "
                      << (probe->boundsMin + pos).z << std::endl;
            std::cout << "Bounds world max: " << (probe->boundsMax + pos).x << ", "
                      << (probe->boundsMax + pos).y << ", "
                      << (probe->boundsMax + pos).z << std::endl;
            probe->cubemap = RenderCubemapFromPoint(pos, probe->resolution);
        }
        for (auto& [probe, pos] : reflectionProbes) {
            if (probe->cubemap != 0)
                glDeleteTextures(1, &probe->cubemap);

            probe->cubemap = RenderCubemapFromPoint(pos, probe->resolution);
            glBindTexture(GL_TEXTURE_CUBE_MAP, probe->cubemap);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            probe->baked = true;

            std::cout << "Baked reflection probe at ("
                      << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
        }
    }

    void Renderer::CollectReflectionProbes() {
        reflectionProbes.clear();
        for (auto& go : scene->objects) {
            if (!go->enabled) continue;
            auto rp = go->GetComponent<ReflectionProbeComponent>();
            if (!rp) continue;
            reflectionProbes.push_back({ rp->probe, go->transform.position });
        }
    }



    void Renderer::ClearProbes() {
        if (probeSSBO != 0) {
            glDeleteBuffers(1, &probeSSBO);
            probeSSBO = 0;
        }
    }