#include "config.h"
#include "mesh.h"
#include "scene.h"
#include "camera.h"
#include "gbuffer.h"
#include "quad.h"
#include "profiler.h"
#include "../dependencies/imgui/imgui.h"
#include "../dependencies/imgui/imgui_impl_glfw.h"
#include "../dependencies/imgui/imgui_impl_opengl3.h"
#define STB_PERLIN_IMPLEMENTATION
#include "../dependencies/stb_perlin.h"

// Shadow settings
constexpr int MAX_SHADOW_LIGHTS = 3;
int SHADOW_RESOLUTION = 2048;
int PCF_KERNEL_SIZE = 3;
bool PCF_ENABLED = true;

bool SSAO_ENABLED = false;

// Fog settings
bool FOG_ENABLED = true;
float FOG_DENSITY = 0.3f;
int FOG_STEPS = 32;
float FOG_SCALE = 0.1f;
float FOG_SCROLL_SPEED = 0.01f;
int FOG_RESOLUTION_SCALE = 2; // 2 = 1/2 resolution, 4 = 1/4, ect...
int FOG_BLUR_KERNEL_SIZE = 3;

Camera* gCamera = nullptr;
float lastMouseX = 960.0f;
float lastMouseY = 540.0f;
float sensitivityX = 5.0f;
float sensitivityY = 5.0f;
bool firstMouse  = true;
bool cursorEnabled = false;
bool tabWasPressed = false;

// Forward declarations
unsigned int MakeShaderModule(const std::string& filePath, unsigned int moduleType);
unsigned int MakeShaderProgram(const std::string& vertexFilepath, const std::string& fragmentFilepath);
std::shared_ptr<Scene> CreateScene();
void GUI(std::shared_ptr<Scene> scene, float deltaTime, Profiler& profiler);
void RenderShadowMap(unsigned int shader, const glm::mat4& lightSpaceMatrix, std::shared_ptr<Scene> scene);
void MouseCallback(GLFWwindow* window, double xpos, double ypos);
unsigned int GenerateNoiseTexture(int size);

int main()
{
    GLFWwindow* window;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW Window" << std::endl;
        return -1;
    }

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    lastMouseX = mode->width  / 2.0f;
    lastMouseY = mode->height / 2.0f;

    window = glfwCreateWindow(mode->width, mode->height, "Penumbra", monitor, NULL);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    // Create GUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui::StyleColorsDark();

    glm::mat4 model      = glm::mat4(1.0f); // identity
    glm::mat4 projection = glm::perspective(
        glm::radians(55.0f),  // FOV
        static_cast<float>(mode->width) / static_cast<float>(mode->height),           // aspect ratio
        0.1f,                        // near plane
        100.0f                       // far plane
    );

    Camera camera(glm::vec3(0.5, 1, 3));
    camera.transform.rotation.y = 250.0f;
    gCamera = &camera;

    Profiler profiler;

    // Lock cursor
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, MouseCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwTerminate();
        return -1;
    }

    glClearColor(0, 0, 0, 1.0f);
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glEnable(GL_DEPTH_TEST);

    GBuffer gbuffer(w, h);
    ScreenQuad quad;

    // Lit
    unsigned int litFBO, litTexture;
    glGenFramebuffers(1, &litFBO);
    glGenTextures(1, &litTexture);
    glBindTexture(GL_TEXTURE_2D, litTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, litFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, litTexture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Fog
    unsigned int fogFBO, fogTexture;
    glGenFramebuffers(1, &fogFBO);
    glGenTextures(1, &fogTexture);
    glBindTexture(GL_TEXTURE_2D, fogTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w/FOG_RESOLUTION_SCALE, h/FOG_RESOLUTION_SCALE, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, fogFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fogTexture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    unsigned int fogNoiseTexture = GenerateNoiseTexture(64);

    unsigned int gBufferShader  = MakeShaderProgram("../shaders/geometry/geometryVert.glsl",  "../shaders/geometry/geometryFrag.glsl");
    unsigned int lightingShader = MakeShaderProgram("../shaders/lighting/lightingVert.glsl", "../shaders/lighting/lightingFrag.glsl");
    unsigned int shadowShader = MakeShaderProgram("../shaders/shadows/shadowVert.glsl", "../shaders/shadows/shadowFrag.glsl");
    unsigned int fogShader = MakeShaderProgram("../shaders/lighting/lightingVert.glsl", "../shaders/fog/fogFrag.glsl");
    unsigned int passthroughShader = MakeShaderProgram("../shaders/lighting/lightingVert.glsl", "../shaders/passthrough/passthroughFrag.glsl");
    unsigned int fogCompositeShader = MakeShaderProgram("../shaders/lighting/lightingVert.glsl", "../shaders/fog/fogCompositeFrag.glsl");

    // Cache geometry shader locs
    int gBuf_model      = glGetUniformLocation(gBufferShader, "model");
    int gBuf_view       = glGetUniformLocation(gBufferShader, "view");
    int gBuf_projection = glGetUniformLocation(gBufferShader, "projection");
    int gBuf_normalMat  = glGetUniformLocation(gBufferShader, "normalMatrix");
    int gBuf_shininess  = glGetUniformLocation(gBufferShader, "shininess");
    int gBuf_diffuseTex = glGetUniformLocation(gBufferShader, "diffuseTex");

    // Cache lighting shader locs
    int light_gPosition  = glGetUniformLocation(lightingShader, "gPosition");
    int light_gNormal    = glGetUniformLocation(lightingShader, "gNormal");
    int light_gAlbedo    = glGetUniformLocation(lightingShader, "gAlbedo");
    int light_cameraPos  = glGetUniformLocation(lightingShader, "cameraPos");
    int light_ambient    = glGetUniformLocation(lightingShader, "ambientMultiplier");

    glUseProgram(lightingShader);
    glUniform1i(light_gPosition, 0);
    glUniform1i(light_gNormal,   1);
    glUniform1i(light_gAlbedo,   2);
    glUniform1f(light_ambient,   0.15f);

    glUseProgram(gBufferShader);
    glUniform1i(gBuf_diffuseTex, 0);
    glUniformMatrix4fv(gBuf_projection, 1, GL_FALSE, glm::value_ptr(projection));

    glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
    glUniformMatrix4fv(gBuf_model,     1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(gBuf_normalMat, 1, GL_FALSE, glm::value_ptr(normalMatrix));

    // Create scene and GameObjects
    std::shared_ptr<Scene> scene = CreateScene();
    // Set default shader
    for (auto& obj : scene->objects) {
        if (obj->mesh)
            obj->mesh->material.shader = gBufferShader; // Default shader
    }
    // Set custom shaders

    // Start scene
    scene->Start();
    int shadowLightCount = 0;

    // Initialize shadowed lights
    for (auto& obj : scene->objects)
    {
        if (!obj->light || !obj->light->castsShadow)
            continue;

        if (shadowLightCount >= MAX_SHADOW_LIGHTS)
            break;

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

        float borderColor[] = {1,1,1,1};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glBindFramebuffer(GL_FRAMEBUFFER, light->shadowFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_TEXTURE_2D, light->shadowMap, 0);

        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        shadowLightCount++;
    }

    double lastTime = glfwGetTime();
    int frameCount = 0;
    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window))
    {
	profiler.Collect();
        float currentTime = glfwGetTime();
        float deltaTime   = currentTime - lastFrame;
        lastFrame         = currentTime;
        frameCount++;

        if (currentTime - lastTime >= 1.0)
        {
            std::string title = "Penumbra | FPS: " + std::to_string(frameCount);
            glfwSetWindowTitle(window, title.c_str());
            frameCount = 0;
            lastTime   = currentTime;
        }

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
	bool tabPressed = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
	if (tabPressed && !tabWasPressed)
	{
		cursorEnabled = !cursorEnabled;

		if (cursorEnabled)
		{
		    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		else
		{
		    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		    firstMouse = true;
		}
	}
	tabWasPressed = tabPressed;
        if (!cursorEnabled)
            camera.ProcessKeyboard(window, deltaTime);
        glm::mat4 view = camera.GetViewMatrix();

        glfwPollEvents();
        scene->Update(deltaTime);

        // -----------------------------------------------
        // PASS 0: REALTIME SHADOWS
        // -----------------------------------------------
	profiler.Begin("Shadow Pass");
        int shadowIndex = 0;
        glUseProgram(shadowShader);
	glDisable(GL_CULL_FACE);

        for (auto& obj : scene->objects) {
		if (!obj->light || !obj->light->castsShadow) continue;
		if (shadowIndex >= MAX_SHADOW_LIGHTS) break;

		auto& light = obj->light;
	        glm::mat4 lightProj, lightView;

		if (light->type == LightType::Directional)
		{
			lightProj = glm::ortho(-8.f, 8.f, -8.f, 8.f, 0.1f, 50.f);

			glm::vec3 up = (glm::abs(light->direction.y) > 0.99f)
			    ? glm::vec3(1, 0, 0)
			    : glm::vec3(0, 1, 0);

			lightView = glm::lookAt(
			    obj->transform.position,
			    obj->transform.position + light->direction,
			    up
			);
		}

		else if (light->type == LightType::Spot)
		{
			lightProj = glm::perspective(
			    glm::acos(light->outerCutoff) * 2.0f, // FOV from cone angle
			    1.0f, 0.1f, 50.f
			);

			glm::vec3 up = (glm::abs(light->direction.y) > 0.99f)
			    ? glm::vec3(1, 0, 0)
			    : glm::vec3(0, 1, 0);

			lightView = glm::lookAt(
			    obj->transform.position,
			    obj->transform.position + light->direction,
			    up
			);
		}

		light->lightSpaceMatrix = lightProj * lightView;

		glViewport(0, 0, SHADOW_RESOLUTION, SHADOW_RESOLUTION);
		glBindFramebuffer(GL_FRAMEBUFFER, light->shadowFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		RenderShadowMap(shadowShader, light->lightSpaceMatrix, scene);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		shadowIndex++;
        }
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
	profiler.End("Shadow Pass");

        // -----------------------------------------------
        // PASS 1: GEOMETRY PASS — render scene into G-Buffer
        // -----------------------------------------------
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

        // -----------------------------------------------
        // PASS 2: LIGHTING PASS — fullscreen quad reads G-Buffer
        // -----------------------------------------------
	profiler.Begin("Lighting Pass");
        glBindFramebuffer(GL_FRAMEBUFFER, litFBO);
	glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(lightingShader);

	glUniform1i(glGetUniformLocation(lightingShader, "pcfKernelSize"), PCF_KERNEL_SIZE);
	glUniform1i(glGetUniformLocation(lightingShader, "shadowLightCount"), shadowIndex);
	glUniform1i(glGetUniformLocation(lightingShader, "pcfEnabled"), PCF_ENABLED ? 1 : 0);

        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gbuffer.gPosition);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gbuffer.gNormal);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, gbuffer.gDiffuse);

	// Shadow maps on slots 3+
	shadowIndex = 0;
	for (auto& obj : scene->objects)
	{
		if (!obj->light || !obj->light->castsShadow) continue;
		if (shadowIndex >= MAX_SHADOW_LIGHTS) break;

		glActiveTexture(GL_TEXTURE3 + shadowIndex);
		glBindTexture(GL_TEXTURE_2D, obj->light->shadowMap);

		std::string name = "shadowMap[" + std::to_string(shadowIndex) + "]";
		glUniform1i(glGetUniformLocation(lightingShader, name.c_str()), 3 + shadowIndex);

		std::string lsm = "lightSpaceMatrix[" + std::to_string(shadowIndex) + "]";
		glUniformMatrix4fv(glGetUniformLocation(lightingShader, lsm.c_str()),
		1, GL_FALSE, glm::value_ptr(obj->light->lightSpaceMatrix));

		shadowIndex++;
	}

        glUniform3f(light_cameraPos,
            camera.transform.position.x,
            camera.transform.position.y,
            camera.transform.position.z);

        scene->UploadLights(lightingShader);
        quad.Draw();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	profiler.End("Lighting Pass");

	if (FOG_ENABLED)
	{
		// -----------------------------------------------
		// PASS 3: VOLUMETRIC FOG PASS
		// -----------------------------------------------
		profiler.Begin("Fog Pass");
		glDisable(GL_DEPTH_TEST);
		glBindFramebuffer(GL_FRAMEBUFFER, fogFBO);
		glViewport(0, 0, w / FOG_RESOLUTION_SCALE, h / FOG_RESOLUTION_SCALE);
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(fogShader);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, litTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, gbuffer.gPosition);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_3D, fogNoiseTexture);

		glUniform1i(glGetUniformLocation(fogShader, "litScene"), 0);
		glUniform1i(glGetUniformLocation(fogShader, "gPosition"), 1);
		glUniform1i(glGetUniformLocation(fogShader, "noiseTexture"), 2);
		glUniform1f(glGetUniformLocation(fogShader, "fogScale"), FOG_SCALE);
		glUniform1f(glGetUniformLocation(fogShader, "fogScrollSpeed"), FOG_SCROLL_SPEED);

		shadowIndex = 0;
		for(auto &obj : scene->objects)
		{
			if(!obj->light || !obj->light->castsShadow)
				continue;
			if(shadowIndex >= MAX_SHADOW_LIGHTS)
				break;
			glActiveTexture(GL_TEXTURE3 + shadowIndex);
			glBindTexture(GL_TEXTURE_2D, obj->light->shadowMap);
			std::string name = "shadowMap[" + std::to_string(shadowIndex) + "]";
			glUniform1i(glGetUniformLocation(fogShader, name.c_str()), 3 + shadowIndex);
			std::string lsm = "lightSpaceMatrix[" + std::to_string(shadowIndex) + "]";
			glUniformMatrix4fv(glGetUniformLocation(fogShader, lsm.c_str()),
			                   1,
			                   GL_FALSE,
			                   glm::value_ptr(obj->light->lightSpaceMatrix));
			shadowIndex++;
		}

		glUniform3f(glGetUniformLocation(fogShader, "cameraPos"),
		            camera.transform.position.x,
		            camera.transform.position.y,
		            camera.transform.position.z);
		glUniform1f(glGetUniformLocation(fogShader, "time"), (float)glfwGetTime());
		glUniform1f(glGetUniformLocation(fogShader, "fogDensity"), FOG_DENSITY);
		glUniform1i(glGetUniformLocation(fogShader, "steps"), FOG_STEPS);
		glUniform1i(glGetUniformLocation(fogShader, "shadowLightCount"), shadowIndex);
		scene->UploadLights(fogShader);
		quad.Draw();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		profiler.End("Fog Pass");

		profiler.Begin("Fog Composite Pass");
		glViewport(0, 0, w, h);
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(fogCompositeShader);
		glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, litTexture);
		glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, fogTexture);
		glUniform1i(glGetUniformLocation(fogCompositeShader, "litScene"), 0);
		glUniform1i(glGetUniformLocation(fogCompositeShader, "fogBuffer"), 1);
		glUniform2f(glGetUniformLocation(fogCompositeShader, "resolution"), (float)w, (float)h);
		glUniform2f(
		    glGetUniformLocation(fogCompositeShader, "fogResolution"),
		    (float)(w / FOG_RESOLUTION_SCALE),
		    (float)(h / FOG_RESOLUTION_SCALE)
		);
		glUniform1i(glGetUniformLocation(fogCompositeShader, "fogBlurKernelSize"), FOG_BLUR_KERNEL_SIZE);
		quad.Draw();
		glEnable(GL_DEPTH_TEST);
		profiler.End("Fog Composite Pass");
	}
	else
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(passthroughShader);
		glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, litTexture);
		glUniform1i(glGetUniformLocation(passthroughShader, "litScene"), 0);
		quad.Draw();
	}
	GUI(scene, deltaTime, profiler);
        glfwSwapBuffers(window);
    }

    glDeleteProgram(gBufferShader);
    glDeleteProgram(lightingShader);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();

    return 0;
}

void RenderShadowMap(unsigned int shader, const glm::mat4& lightSpaceMatrix, std::shared_ptr<Scene> scene)
{
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

    for (auto& obj : scene->objects)
    {
        if (!obj->mesh || !obj->enabled)
            continue;

        glm::mat4 model = obj->transform.GetMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        obj->mesh->Draw();
    }
}

void GUI(std::shared_ptr<Scene> scene, float deltaTime, Profiler& profiler)
{
    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Performance window
    ImGui::Begin("Performance");
    ImGui::Text("FPS: %.0f", 1.0f / deltaTime);
    ImGui::Text("Frame Time: %.2f ms", deltaTime * 1000.0f);
    ImGui::Separator();
    profiler.DrawGUI();
    ImGui::End();

    // Graphics Settings
    ImGui::Begin("Graphics Settings");
    ImGui::Checkbox("Volumetric Fog", &FOG_ENABLED);
    if (FOG_ENABLED) {
	ImGui::SliderFloat("Density", &FOG_DENSITY, 0.0f, 2.0f);
	ImGui::SliderInt("Steps", &FOG_STEPS, 4, 64);
	ImGui::SliderFloat("Scale", &FOG_SCALE, 0.01f, 2.0f);
	ImGui::SliderFloat("Scroll Speed", &FOG_SCROLL_SPEED, 0.01f, 2.0f);
	ImGui::SliderInt("Fog Blur", &FOG_BLUR_KERNEL_SIZE, 1, 9);
	if (FOG_BLUR_KERNEL_SIZE % 2 == 0) FOG_BLUR_KERNEL_SIZE++;
    }
    ImGui::Checkbox("PCF shadows", &PCF_ENABLED);
    if (PCF_ENABLED)
        ImGui::SliderInt("PCF Kernel Size", &PCF_KERNEL_SIZE, 1, 9);
    ImGui::End();

    // Scene editor
    ImGui::Begin("Scene");
    ImGuiIO& io = ImGui::GetIO();
    for (std::shared_ptr<GameObject> obj : scene->objects) {
        ImGui::Checkbox(obj->name.c_str(), &obj->enabled);
    }
    ImGui::Separator();
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

std::shared_ptr<Scene> CreateScene()
{
    std::shared_ptr<Scene> scene = std::make_shared<Scene>();

    std::shared_ptr<GameObject> wall = std::make_shared<GameObject>("Wall");
    wall->mesh = std::make_shared<Mesh>("../assets/models/walls.obj", "../assets/textures/scene.png", 32.0);
    scene->Add(wall);

    std::shared_ptr<GameObject> wall2 = std::make_shared<GameObject>("Wall 2");
    wall2->mesh = std::make_shared<Mesh>("../assets/models/walls.obj", "../assets/textures/scene.png", 32.0);
    wall2->transform.rotation = glm::vec3(0.0f, 180.0f, 0.0f);
    scene->Add(wall2);

    std::shared_ptr<GameObject> table = std::make_shared<GameObject>("Table");
    table->mesh = std::make_shared<Mesh>("../assets/models/table.obj", "../assets/textures/scene.png", 512.0);
    scene->Add(table);

    std::shared_ptr<GameObject> floor = std::make_shared<GameObject>("Floor");
    floor->mesh = std::make_shared<Mesh>("../assets/models/floor.obj", "../assets/textures/scene.png", 512.0);
    scene->Add(floor);

    std::shared_ptr<GameObject> chairTops = std::make_shared<GameObject>("Chair Tops");
    chairTops->mesh = std::make_shared<Mesh>("../assets/models/chairTops.obj", "../assets/textures/scene.png", 12.0);
    scene->Add(chairTops);

    std::shared_ptr<GameObject> chairBottoms = std::make_shared<GameObject>("Chair Bottoms");
    chairBottoms->mesh = std::make_shared<Mesh>("../assets/models/chairBottoms.obj", "../assets/textures/scene.png", 64.0);
    scene->Add(chairBottoms);

    std::shared_ptr<GameObject> directionalLight = std::make_shared<GameObject>("Directional Light");
    directionalLight->light = std::make_shared<Light>();
    directionalLight->light->color = glm::vec3(1.0f, 1.0f, 1.0f);
    directionalLight->light->intensity = 1.0f;
    directionalLight->light->castsShadow = true;
    directionalLight->light->intensity = 0.1f;
    directionalLight->light->type = LightType::Directional;
    directionalLight->light->direction = glm::vec3(0.0f, -1.0f, 0.0f);
    directionalLight->transform.position = glm::vec3(1.0f, 10.0f, 2.0f);
    scene->Add(directionalLight);

    std::shared_ptr<Orbiter> orb = std::make_shared<Orbiter>("Spotlight");
    orb->light = std::make_shared<Light>();
    orb->light->color = glm::vec3(1.0f, 0.0f, 0.0f);
    orb->orbitCenter  = glm::vec3(0.0f, 0.25f, -1.5f);
    orb->light->intensity = 2.0f;
    orb->light->castsShadow = true;
    orb->light->type = LightType::Spot;
    orb->light->direction = glm::vec3(0.0f, 0.0f, 1.0f);
    orb->orbitRadius = 0.25f;
    orb->orbitSpeed = 45.0f;
    scene->Add(orb);

    std::shared_ptr<Orbiter> orb2 = std::make_shared<Orbiter>("Orbiter 2");
    orb2->light = std::make_shared<Light>();
    orb2->light->color  = glm::vec3(0.0f, 1.0f, 0.0f);
	orb2->orbitCenter  = glm::vec3(0.0f, 2.25f, 0.0f);
	orb2->light->intensity = 1.0f;
	orb2->light->castsShadow = true;
	orb2->light->type = LightType::Spot;
    orb2->orbitRadius   = 0.25f;
    orb2->orbitSpeed    = 25.0f;
    scene->Add(orb2);

    std::shared_ptr<Orbiter> orb3 = std::make_shared<Orbiter>("Orbiter 3");
    orb3->light = std::make_shared<Light>();
    orb3->orbitCenter  = glm::vec3(0.0f, 2.25f, 0.0f);
    orb3->light->color  = glm::vec3(0.0f, 0.0f, 1.0f);
	orb3->light->intensity = 1.0f;
	orb3->light->castsShadow = true;
	orb3->light->type = LightType::Spot;
    orb3->orbitRadius   = 1.15f;
    orb3->orbitSpeed    = 35.0f;
    scene->Add(orb3);

    return scene;
}

unsigned int MakeShaderModule(const std::string& filePath, unsigned int moduleType)
{
    std::ifstream file;
    std::stringstream ss;
    std::string line;

    file.open(filePath);
    if (!file.is_open())
    {
	std::cerr << "Failed to open shader file: " << filePath << std::endl;
	return 0;
    }
    while (std::getline(file, line)) {
        ss << line << "\n";
    }

    // Dump string stream into a string and convert to C style char array
    std::string shaderSource = ss.str();
    const char* shaderSourcePtr = shaderSource.c_str();
    ss.str("");
    file.close();

    unsigned int shader = glCreateShader(moduleType);
    glShaderSource(shader, 1, &shaderSourcePtr, NULL);
    glCompileShader(shader);

    // Error check
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char errorLog[1024];
        glGetShaderInfoLog(shader, 1024, NULL, errorLog);
        std::cout << "Shader compilation error: \n" << errorLog << std::endl;
    }

    return shader;
}

unsigned int MakeShaderProgram(const std::string& vertexFilepath, const std::string& fragmentFilepath)
{
    std::vector<unsigned int> modules;
    // Create Vertex and Fragment Modules
    modules.push_back(MakeShaderModule(vertexFilepath, GL_VERTEX_SHADER));
    modules.push_back(MakeShaderModule(fragmentFilepath, GL_FRAGMENT_SHADER));

    // Create shader program and attach modules
    unsigned int program = glCreateProgram();
    for (unsigned module : modules)
    {
        glAttachShader(program, module);
    }

    glLinkProgram(program);

    // Error check
    int success;

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char errorLog[1024];
        glGetProgramInfoLog(program, 1024, NULL, errorLog);
        std::cout << "Program link error: \n" << errorLog << std::endl;
    }

    // Shader modules not needed after linking
    for (unsigned module : modules)
    {
        glDeleteShader(module);
    }

    return program;
}

void MouseCallback(GLFWwindow* window, double xpos, double ypos)
{
	ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
	if (firstMouse) {
		lastMouseX = xpos;
		lastMouseY = ypos;
		firstMouse = false;
	}
	float xOffset = (xpos - lastMouseX) * sensitivityX;
	float yOffset = (lastMouseY - ypos) * sensitivityY;
	lastMouseX = xpos;
	lastMouseY = ypos;
	if (!cursorEnabled)
		gCamera->ProcessMouse(xOffset, yOffset);
}

unsigned int GenerateNoiseTexture(int size)
{
	std::vector<float> data(size * size * size);
	for (int z = 0; z < size; z++)
		for (int y = 0; y < size; y++)
			for (int x = 0; x < size; x++)
			{
				float fx = (float)x / size;
				float fy = (float)y / size;
				float fz = (float)z / size;

				// Layer multiple octaves for cloud-like noise
				float n = 0.0f;
				n += 1.000f * stb_perlin_noise3(fx * 2,  fy * 2,  fz * 2,  2,  2,  2);
				n += 0.500f * stb_perlin_noise3(fx * 4,  fy * 4,  fz * 4,  4,  4,  4);
				n += 0.250f * stb_perlin_noise3(fx * 8,  fy * 8,  fz * 8,  8,  8,  8);
				n += 0.125f * stb_perlin_noise3(fx * 16, fy * 16, fz * 16, 16, 16, 16);

				// Remap from [-1,1] to [0,1]
				n = n * 0.5f + 0.5f;
				n = glm::clamp(n, 0.0f, 1.0f);

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