    #include "renderer.h"
    #define STB_PERLIN_IMPLEMENTATION
    #include "../../dependencies/stb_perlin.h"
    #include "effects/particles/particleSystemComponent.h"
    #include "effects/reflections/reflectionProbeComponent.h"


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
        glDeleteProgram(skyboxShader);
        glDeleteProgram(fxaaShader);

        glDeleteFramebuffers(1, &fxaaFBO);
        glDeleteTextures(1, &fxaaTexture);
        glDeleteProgram(fxaaShader);
        glDeleteProgram(ssaoBlurShader);
    }

    void Renderer::CreateFBOS() {
        CreateLitFBO();
        CreateFogFBO();
        CreateFXAA();
        CreateSSAO();
        CreateSSR();
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
                    mat.shader = gBufferShader;
            }
        }
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

    void Renderer::CreateSSR() {
        // SSR FBO
        glGenFramebuffers(1, &ssrFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, ssrFBO);

        glGenTextures(1, &ssrTexture);
        glBindTexture(GL_TEXTURE_2D, ssrTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssrTexture, 0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "SSR FBO incomplete!" << std::endl;

        // SSR Composite
        glGenFramebuffers(1, &ssrCompositeFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, ssrCompositeFBO);

        glGenTextures(1, &ssrCompositeTexture);
        glBindTexture(GL_TEXTURE_2D, ssrCompositeTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssrCompositeTexture, 0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "SSR Composite FBO incomplete!" << std::endl;

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

    void Renderer::LoadShaders() {
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
        pointShadowShader = ShaderUtils::MakeShaderProgram("../shaders/shadows/pointShadowVert.glsl", "../shaders/shadows/pointShadowFrag.glsl");
        skyboxShader = ShaderUtils::MakeShaderProgram("../shaders/skybox/skyboxVert.glsl","../shaders/skybox/skyboxFrag.glsl");
        probeBakeShader = ShaderUtils::MakeShaderProgram("../shaders/lighting/probeBakeVert.glsl", "../shaders/lighting/probeBakeFrag.glsl");
        ssrShader = ShaderUtils::MakeShaderProgram("../shaders/ssr/ssrVert.glsl", "../shaders/ssr/ssrFrag.glsl");
        ssrCompositeShader = ShaderUtils::MakeShaderProgram("../shaders/ssr/ssrVert.glsl", "../shaders/ssr/ssrCompositeFrag.glsl");
    }

    void Renderer::CacheUniforms()
    {
        // Geometry shader
        gBuf_model      = glGetUniformLocation(gBufferShader, "model");
        gBuf_view       = glGetUniformLocation(gBufferShader, "view");
        gBuf_projection = glGetUniformLocation(gBufferShader, "projection");
        gBuf_normalMat  = glGetUniformLocation(gBufferShader, "normalMatrix");
        gBuf_roughness = glGetUniformLocation(gBufferShader, "roughness");
        gBuf_metallic  = glGetUniformLocation(gBufferShader, "metallic");
        gBuf_diffuseTex = glGetUniformLocation(gBufferShader, "diffuseTex");
        gBuf_normalMap  = glGetUniformLocation(gBufferShader, "normalMap");
        gBuf_hasNormalMap = glGetUniformLocation(gBufferShader, "hasNormalMap");

        glUseProgram(gBufferShader);
        glUniform1i(gBuf_diffuseTex, 0);
        glUniform1i(gBuf_normalMap, 1);
        glUniform1i(glGetUniformLocation(gBufferShader, "heightMap"), 2);
        glUniform1i(glGetUniformLocation(gBufferShader, "metallicRoughnessMap"), 3);
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
        glUniform1f(light_ambient, AMBIENT_MULTIPLIER);

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
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, litFBO);
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, litFBO);

        if (SSR_ENABLED) {
            SSRPass();
            SSRCompositePass();

            glBindFramebuffer(GL_READ_FRAMEBUFFER, ssrCompositeFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, litFBO);
            glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }

        SkyboxPass();
        ParticlePass(shadowCount);
        WaterPass(deltaTime);

        if (FOG_ENABLED) FogPass(shadowCount);
        else PassthroughPass();
        FXAAPass();
    }

    void Renderer::ShadowPass(bool staticOnly)
    {
        profiler.Begin("Shadow Pass");
        int idx = 0;
        glUseProgram(shadowShader);
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
                glUseProgram(pointShadowShader);
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

                glUniform3fv(glGetUniformLocation(pointShadowShader, "lightPos"), 1, glm::value_ptr(pos));
                glUniform1f(glGetUniformLocation(pointShadowShader, "farPlane"), far);

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

                    glUniformMatrix4fv(glGetUniformLocation(pointShadowShader, "lightSpaceMatrix"),
                                       1, GL_FALSE, glm::value_ptr(shadowTransforms[face]));

                    for (auto& renderObj : scene->objects)
                    {
                        auto mc = renderObj->GetComponent<MeshComponent>();
                        if (!mc || !renderObj->enabled) continue;
                        if (renderObj->IsForwardRendered()) continue;
                        glm::mat4 model = renderObj->transform.GetMatrix();
                        glUniformMatrix4fv(glGetUniformLocation(pointShadowShader, "model"),
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
                glUseProgram(shadowShader);
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
        glUseProgram(gBufferShader);
        glUniformMatrix4fv(gBuf_view, 1, GL_FALSE, glm::value_ptr(view));
        glUniform3f(glGetUniformLocation(gBufferShader, "viewPos"),
                camera.transform.position.x,
                camera.transform.position.y,
                camera.transform.position.z);
        scene->RenderGeometry(gBufferShader, gBuf_model);
        gbuffer.Unbind();
        profiler.End("Geometry Pass");
    }

    void Renderer::SSAOPass(const glm::mat4& view)
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

    void Renderer::LightingPass(int shadowCount)
    {
        profiler.Begin("Lighting Pass");
        glBindFramebuffer(GL_FRAMEBUFFER, litFBO);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(lightingShader);

        glUniform1f(light_ambient, AMBIENT_MULTIPLIER);

        glUniform1i(glGetUniformLocation(lightingShader, "pcfKernelSize"), PCF_KERNEL_SIZE);
        glUniform1i(glGetUniformLocation(lightingShader, "shadowLightCount"), shadowCount);
        glUniform1i(glGetUniformLocation(lightingShader, "pcfEnabled"), PCF_ENABLED ? 1 : 0);
        glUniform1f(glGetUniformLocation(lightingShader, "shadowBias"), SHADOW_BIAS);
        glUniform1f(glGetUniformLocation(lightingShader, "shadowNormalOffset"), SHADOW_NORMAL_OFFSET);
        glUniform1f(glGetUniformLocation(lightingShader, "pointShadowFarPlane"), POINT_SHADOW_FAR_PLANE);

        // ------------------------
        // GBUFFER (LOCKED SLOTS)
        // ------------------------
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gbuffer.gPosition);
        glUniform1i(glGetUniformLocation(lightingShader, "gPosition"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gbuffer.gNormal);
        glUniform1i(glGetUniformLocation(lightingShader, "gNormal"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gbuffer.gAlbedo);
        glUniform1i(glGetUniformLocation(lightingShader, "gAlbedo"), 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, gbuffer.gEmissive);
        glUniform1i(glGetUniformLocation(lightingShader, "gEmissive"), 3);

        // ------------------------
        // SHADOW MAPS START HERE
        // ------------------------
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
            glUniform1i(glGetUniformLocation(lightingShader, name.c_str()), baseSlot + si);

            std::string lsm = "lightSpaceMatrix[" + std::to_string(si) + "]";
            glUniformMatrix4fv(glGetUniformLocation(lightingShader, lsm.c_str()),
                               1, GL_FALSE, glm::value_ptr(lc->light->lightSpaceMatrix));

            si++;
        }

        // ------------------------
        // CUBEMAPS
        // ------------------------
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
            glUniform1i(glGetUniformLocation(lightingShader, name.c_str()), cubeStart + ci);

            ci++;
        }

        // ------------------------
        // FAR PLANES
        // ------------------------
        int fi = 0;
        for (auto& obj : scene->objects)
        {
            auto lc = obj->GetComponent<LightComponent>();
            if (!lc || !lc->light->castsShadow || !obj->enabled) continue;
            if (fi >= MAX_SHADOW_LIGHTS) break;

            float fp = (lc->light->type == LightType::Point) ? lc->light->radius : 50.0f;

            std::string name = "lightFarPlane[" + std::to_string(fi) + "]";
            glUniform1f(glGetUniformLocation(lightingShader, name.c_str()), fp);

            fi++;
        }

        // ------------------------
        // SSAO (LAST SLOT)
        // ------------------------
        int ssaoSlot = cubeStart + ci;

        glActiveTexture(GL_TEXTURE0 + ssaoSlot);
        glBindTexture(GL_TEXTURE_2D, SSAO_ENABLED ? ssaoBlurTexture : 0);

        glUniform1i(glGetUniformLocation(lightingShader, "ssaoTexture"), ssaoSlot);
        glUniform1i(glGetUniformLocation(lightingShader, "ssaoEnabled"), SSAO_ENABLED ? 1 : 0);

        // ------------------------
        // CAMERA
        // ------------------------
        glUniform3f(light_cameraPos,
                    camera.transform.position.x,
                    camera.transform.position.y,
                    camera.transform.position.z);

        scene->UploadLights(lightingShader);

        glUniform1i(glGetUniformLocation(lightingShader, "giMode"), GI_MODE);
        glUniform1f(glGetUniformLocation(lightingShader, "giIntensity"), GI_INTENSITY);

        if (GI_MODE >= 1 && probeSSBO != 0) {
            auto& grid = scene->probeGrid;
            glUniform3fv(glGetUniformLocation(lightingShader, "probeGridMin"),
                         1, glm::value_ptr(grid.boundsMin));
            glUniform3fv(glGetUniformLocation(lightingShader, "probeGridMax"),
                         1, glm::value_ptr(grid.boundsMax));
            glUniform3i(glGetUniformLocation(lightingShader, "probeGridCount"),
                        grid.countX, grid.countY, grid.countZ);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, probeSSBO);
        }

        quad.Draw();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        profiler.End("Lighting Pass");
    }

    void Renderer::FogPass(int shadowCount)
    {
        // Volumetric fog
        profiler.Begin("Fog Pass");
        glDisable(GL_DEPTH_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, fogFBO);
        glViewport(0, 0, w / FOG_RESOLUTION_SCALE, h / FOG_RESOLUTION_SCALE);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(fogShader);

        scene->UploadFogVolumes(fogShader);

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
            auto lc = obj->GetComponent<LightComponent>();
            if (!lc || !lc->light->castsShadow || !obj->enabled) continue;
            if (si >= MAX_SHADOW_LIGHTS) break;

            glActiveTexture(GL_TEXTURE3 + si);
            if (lc->light->type == LightType::Point)
                glBindTexture(GL_TEXTURE_2D, 0);
            else
                glBindTexture(GL_TEXTURE_2D, lc->light->shadowMap);

            std::string name = "shadowMap[" + std::to_string(si) + "]";
            glUniform1i(glGetUniformLocation(fogShader, name.c_str()), 3 + si);
            std::string lsm = "lightSpaceMatrix[" + std::to_string(si) + "]";
            glUniformMatrix4fv(glGetUniformLocation(fogShader, lsm.c_str()),
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
            glUniform1i(glGetUniformLocation(fogShader, name.c_str()), cubeStart + ci);
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
            glUniform1f(glGetUniformLocation(fogShader, name.c_str()), fp);
            fi++;
        }

        glUniform1f(glGetUniformLocation(fogShader, "pointShadowFarPlane"), POINT_SHADOW_FAR_PLANE);

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

    void Renderer::FXAAPass()
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

    void Renderer::SSRPass()
    {
        profiler.Begin("SSR Pass");

        glBindFramebuffer(GL_FRAMEBUFFER, ssrFBO);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(ssrShader);

        // gAlbedo
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gbuffer.gAlbedo);
        glUniform1i(glGetUniformLocation(ssrShader, "gAlbedo"), 0);
        // gNormal
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gbuffer.gNormal);
        glUniform1i(glGetUniformLocation(ssrShader, "gNormal"), 1);
        // gPosition
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gbuffer.gPosition);
        glUniform1i(glGetUniformLocation(ssrShader, "gPosition"), 2);
        // sceneColor (lit buffer)
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, litTexture);
        glUniform1i(glGetUniformLocation(ssrShader, "sceneColor"), 3);

        glUniform3fv(glGetUniformLocation(ssrShader, "cameraPos"), 1, &camera.transform.position[0]);
        glUniformMatrix4fv(glGetUniformLocation(ssrShader, "view"), 1, GL_FALSE, &camera.GetViewMatrix()[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(ssrShader, "projection"), 1, GL_FALSE, &projection[0][0]);
        glUniform1f(glGetUniformLocation(ssrShader, "minStepSize"), SSR_MIN_STEP_SIZE);
        glUniform1f(glGetUniformLocation(ssrShader, "maxStepSize"), SSR_MAX_STEP_SIZE);
        glUniform1i(glGetUniformLocation(ssrShader, "raymarchSteps"), SSR_RAYMARCH_STEPS);

        quad.Draw();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        profiler.End("SSR Pass");
    }

    void Renderer::SSRCompositePass() {
        profiler.Begin("SSR Composite Pass");

        glBindFramebuffer(GL_FRAMEBUFFER, ssrCompositeFBO);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(ssrCompositeShader);

        glUniform1i(glGetUniformLocation(ssrCompositeShader, "ssrEnabled"), SSR_ENABLED ? 1 : 0);
        glUniform1i(glGetUniformLocation(ssrCompositeShader, "probeEnabled"), REFLECTION_PROBE_ENABLED ? 1 : 0);

        // Upload all needed textures from gbuffer
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, litTexture);
        glUniform1i(glGetUniformLocation(ssrCompositeShader, "litScene"), 0);

        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, ssrTexture);
        glUniform1i(glGetUniformLocation(ssrCompositeShader, "ssrTexture"), 1);

        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, gbuffer.gNormal);
        glUniform1i(glGetUniformLocation(ssrCompositeShader, "gNormal"), 2);

        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, gbuffer.gAlbedo);
        glUniform1i(glGetUniformLocation(ssrCompositeShader, "gAlbedo"), 3);

        glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, gbuffer.gPosition);
        glUniform1i(glGetUniformLocation(ssrCompositeShader, "gPosition"), 4);

        glUniform3fv(glGetUniformLocation(ssrCompositeShader, "cameraPos"), 1, &camera.transform.position[0]);

        // Upload Reflection probe textures at slot 5+
        int probeCount = REFLECTION_PROBE_ENABLED
            ? (int)std::min(reflectionProbes.size(), (size_t)MAX_REFLECTION_PROBES)
            : 0;

        glUniform1i(glGetUniformLocation(ssrCompositeShader, "probeCount"), probeCount);

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

            glUniform1i(glGetUniformLocation(ssrCompositeShader, slot.c_str()), 5 + i);
            glUniform3fv(glGetUniformLocation(ssrCompositeShader, bMin.c_str()), 1, glm::value_ptr(worldMin));
            glUniform3fv(glGetUniformLocation(ssrCompositeShader, bMax.c_str()), 1, glm::value_ptr(worldMax));
            glUniform3fv(glGetUniformLocation(ssrCompositeShader, posU.c_str()), 1, glm::value_ptr(pos));
            glUniform1f(glGetUniformLocation(ssrCompositeShader,  br.c_str()),   probe->blendRadius);
        }

        if (scene->skybox) {
            int skyboxSlot = 5 + probeCount;
            glActiveTexture(GL_TEXTURE0 + skyboxSlot);
            glBindTexture(GL_TEXTURE_CUBE_MAP, scene->skybox->GetCubemapID());
            glUniform1i(glGetUniformLocation(ssrCompositeShader, "skybox"), skyboxSlot);
            glUniform1i(glGetUniformLocation(ssrCompositeShader, "hasSkybox"), 1);
        } else {
            glUniform1i(glGetUniformLocation(ssrCompositeShader, "hasSkybox"), 0);
        }

        quad.Draw();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        profiler.End("SSR Composite Pass");
    }

    void Renderer::ParticlePass(int shadowCount)
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
            if (!obj->enabled) continue;

            auto ps = obj->GetComponent<ParticleSystemComponent>();
            if (!ps) continue;

            unsigned int shader = ps->system->IsLit() ? particleLitShader : particleUnlitShader;
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
        glUseProgram(skyboxShader);

        glm::mat4 view = glm::mat4(glm::mat3(camera.GetViewMatrix()));

        glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "view"),
                           1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "projection"),
                           1, GL_FALSE, glm::value_ptr(projection));
        glUniform1i(glGetUniformLocation(skyboxShader, "skybox"), 0);

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
        glBindFramebuffer(GL_FRAMEBUFFER, litFBO);
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
        glUseProgram(shadowShader);
        glUniformMatrix4fv(glGetUniformLocation(shadowShader, "lightSpaceMatrix"),
                           1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

        for (auto& obj : scene->objects)
        {
            auto mc = obj->GetComponent<MeshComponent>();
            if (!mc || !obj->enabled) continue;
            if (obj->IsForwardRendered()) continue;
            glm::mat4 model = obj->transform.GetMatrix();
            glUniformMatrix4fv(glGetUniformLocation(shadowShader, "model"),
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
                    auto lc = obj->GetComponent<LightComponent>();
                    if (!lc || !lc->light->castsShadow) continue;
                    if (idx == mode - 6) return lc->light->shadowMap;
                    idx++;
                }
                return 0;
            }
            case 9: return fogTexture;
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

        int bake_modelLoc     = glGetUniformLocation(probeBakeShader, "model");
        int bake_viewLoc      = glGetUniformLocation(probeBakeShader, "view");
        int bake_projLoc      = glGetUniformLocation(probeBakeShader, "projection");
        int bake_normalMatLoc = glGetUniformLocation(probeBakeShader, "normalMatrix");
        int bake_cameraPosLoc = glGetUniformLocation(probeBakeShader, "cameraPos");
        int bake_ambientLoc   = glGetUniformLocation(probeBakeShader, "ambientMultiplier");
        int bake_roughnessLoc = glGetUniformLocation(probeBakeShader, "roughness");
        int bake_metallicLoc  = glGetUniformLocation(probeBakeShader, "metallic");
        int bake_diffuseTexLoc = glGetUniformLocation(probeBakeShader, "diffuseTex");
        int bake_shadowCountLoc = glGetUniformLocation(probeBakeShader, "shadowLightCount");

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
            glUseProgram(probeBakeShader);
            glUniformMatrix4fv(bake_projLoc, 1, GL_FALSE, glm::value_ptr(captureProj));
            glUniformMatrix4fv(bake_viewLoc, 1, GL_FALSE, glm::value_ptr(captureViews[face]));
            glUniform3fv(bake_cameraPosLoc, 1, glm::value_ptr(position));
            glUniform1f(bake_ambientLoc, AMBIENT_MULTIPLIER);
            glUniform1i(bake_diffuseTexLoc, 0);

            // Upload lights
            scene->UploadLights(probeBakeShader, true);

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
                glUniform1i(glGetUniformLocation(probeBakeShader, name.c_str()), 1 + si);
                std::string lsm = "lightSpaceMatrix[" + std::to_string(si) + "]";
                glUniformMatrix4fv(glGetUniformLocation(probeBakeShader, lsm.c_str()),
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
                glUniform1i(glGetUniformLocation(probeBakeShader, name.c_str()), cubeStart + ci);
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
                glUniform1f(glGetUniformLocation(probeBakeShader, name.c_str()), fp);
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
                glUseProgram(skyboxShader);

                glm::mat4 skyView = glm::mat4(glm::mat3(captureViews[face]));
                glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "view"),
                                   1, GL_FALSE, glm::value_ptr(skyView));
                glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "projection"),
                                   1, GL_FALSE, glm::value_ptr(captureProj));
                glUniform1i(glGetUniformLocation(skyboxShader, "skybox"), 0);

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