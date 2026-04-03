#include "config.h"
#include "mesh.h"
#include "scene.h"
#include "camera.h"
#include "renderer.h"
#include "profiler.h"
#include "particles/particleSystem.h"
#include "particles/particleSystemManager.h"
#include "../dependencies/imgui/imgui.h"
#include "../dependencies/imgui/imgui_impl_glfw.h"
#include "../dependencies/imgui/imgui_impl_opengl3.h"

Camera* gCamera = nullptr;
float lastMouseX = 960.0f;
float lastMouseY = 540.0f;
float sensitivityX = 5.0f;
float sensitivityY = 5.0f;
bool firstMouse  = true;
bool cursorEnabled = false;
bool tabWasPressed = false;

std::shared_ptr<Scene> CreateScene();
void GUI(std::shared_ptr<Scene> scene, float deltaTime, Profiler& profiler, Renderer& renderer);
void MouseCallback(GLFWwindow* window, double xpos, double ypos);

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

    glm::mat4 projection = glm::perspective(
        glm::radians(55.0f),
        static_cast<float>(mode->width) / static_cast<float>(mode->height),
        0.1f,
        100.0f
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

	//--------------------------
    // Create renderer and scene
	// -------------------------
    Renderer renderer(w, h, projection);
    std::shared_ptr<Scene> scene = CreateScene();
    renderer.AssignDefaultShader(scene);
    scene->Start();
    renderer.InitShadowMaps(scene);

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
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            else
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
        }
        tabWasPressed = tabPressed;

        if (!cursorEnabled)
            camera.ProcessKeyboard(window, deltaTime);

        glfwPollEvents();
        scene->Update(deltaTime);

        renderer.RenderFrame(camera, scene, profiler);

        GUI(scene, deltaTime, profiler, renderer);
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();

    return 0;
}

void GUI(std::shared_ptr<Scene> scene, float deltaTime, Profiler& profiler, Renderer& renderer)
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

	ImGui::Checkbox("SSAO", &SSAO_ENABLED);
	if (SSAO_ENABLED) {
		ImGui::SliderFloat("SSAO Radius", &SSAO_RADIUS, 0.1f, 2.0f);
		ImGui::SliderFloat("SSAO Bias", &SSAO_BIAS, 0.0f, 0.1f);
		ImGui::SliderInt("SSAO Samples", &SSAO_KERNEL_SIZE, 8, 64);
	}

	ImGui::Checkbox("FXAA", &FXAA_ENABLED);

    ImGui::Checkbox("PCF shadows", &PCF_ENABLED);
    if (PCF_ENABLED)
        ImGui::SliderInt("PCF Kernel Size", &PCF_KERNEL_SIZE, 1, 9);

    ImGui::End();


	ImGui::Begin("Buffer Preview");
	{
    	static int selected = -1;
    	const char* buffers[] = {
    		"Position", "Normals", "Diffuse",
			"SSAO", "SSAO Blurred",
			"Lighting",
			"Shadow Map 0", "Shadow Map 1", "Shadow Map 2",
			"Fog"
		};
    	ImGui::Combo("Buffer", &selected, buffers, IM_ARRAYSIZE(buffers));

    	if (selected >= 0)
    	{
    		unsigned int tex = renderer.GetDebugTexture(selected, scene);
    		if (tex != 0)
    		{
    			float previewWidth = ImGui::GetContentRegionAvail().x;
    			float aspect = 9.0f / 16.0f;
    			ImVec2 size(previewWidth, previewWidth * aspect);
    			// Flip Y since OpenGL textures are bottom-up
    			ImVec2 pos = ImGui::GetCursorScreenPos();
    			ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(0, 0, 0, 255));
    			ImGui::Image((ImTextureID)(intptr_t)tex, size, ImVec2(0, 1), ImVec2(1, 0));
    		}
    		else
    		{
    			ImGui::TextDisabled("Buffer not available");
    		}
    	}
	}
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

	std::shared_ptr<GameObject> corridor = std::make_shared<GameObject>("Corridor");
	corridor->mesh = std::make_shared<Mesh>(
		"../assets/models/corridor.obj",
		"../assets/textures/corridor.jpg",
		0,
		"../assets/textures/normals/corridorNormal.jpg"
		);
	scene->Add(corridor);

	std::shared_ptr<GameObject> cables = std::make_shared<GameObject>("Cables");
	cables->mesh = std::make_shared<Mesh>(
		"../assets/models/cables.obj",
		"../assets/textures/rubber.jpg",
		32,
		"../assets/textures/normals/rubberNormal.jpg"
		);
	scene->Add(cables);

	std::shared_ptr<GameObject> cableHolder = std::make_shared<GameObject>("Cable Holder");
	cableHolder->mesh = std::make_shared<Mesh>(
		"../assets/models/cableholder.obj",
		"../assets/textures/metal.jpg",
		12,
		"../assets/textures/normals/metalNormal.jpg"
		);
	scene->Add(cableHolder);

	std::shared_ptr<GameObject> barrels = std::make_shared<GameObject>("Barrels");
	barrels->mesh = std::make_shared<Mesh>(
		"../assets/models/barrels.obj",
		"../assets/textures/barrel.png",
		12,
		"../assets/textures/normals/barrelNormal.png"
		);
	scene->Add(barrels);

	std::shared_ptr<GameObject> debris = std::make_shared<GameObject>("Debris");
	debris->mesh = std::make_shared<Mesh>(
		"../assets/models/debris.obj",
		"../assets/textures/barrel.png",
		12,
		"../assets/textures/normals/metalNormal.jpg"
		);
	scene->Add(debris);

	std::shared_ptr<Orbiter> light = std::make_shared<Orbiter>("Red Light");
	light->light = std::make_shared<Light>();
	//light->rotationSpeed = 30.0f;
	light->light->color = glm::vec3(1.0f, 0.0f, 0.0f);
	light->light->intensity = 1.0f;
	light->light->type = LightType::Spot;
	light->light->castsShadow = true;
	light->light->direction = glm::vec3(-1.0f, 0.0f, 0.0f);
	light->transform.position = glm::vec3(-6.0f, 0.25f, -0.015f);
	light->orbitCenter = glm::vec3(-6.0f, 0.25f, -0.015f);
	light->orbitRadius = 0.2f;
	scene->Add(light);

	std::shared_ptr<Orbiter> light2 = std::make_shared<Orbiter>("Green Light");
	light2->light = std::make_shared<Light>();
	//light->rotationSpeed = 30.0f;
	light2->light->color = glm::vec3(0.0f, 1.0f, 0.0f);
	light2->light->intensity = 1.0f;
	light2->light->type = LightType::Spot;
	light2->light->castsShadow = true;
	light2->light->direction = glm::vec3(1.0f, 0.0f, 0.0f);
	light2->transform.position = glm::vec3(-12.0f, 0.4f, -0.03f);
	light2->orbitCenter = glm::vec3(-15.0f, 0.25f, -0.03f);
	light2->orbitRadius = 0.2f;
	scene->Add(light2);

	// Particles
	std::shared_ptr<GameObject> particles = std::make_shared<GameObject>("Particles");
	particles->particleSystem = std::make_shared<ParticleSystem>(particles->transform.position, 1000);
	ParticleSystemManager::Instance().Register(particles->particleSystem);
	scene->Add(particles);

    return scene;
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
