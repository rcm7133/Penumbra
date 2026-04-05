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

Profiler profiler;
ParticleSystemManager particleManager;

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

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

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

    Camera camera(glm::vec3(-8, 1, 1));
    camera.transform.rotation.y = 250.0f;
    gCamera = &camera;

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
    	particleManager.Update(deltaTime);

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

	ImGui::SliderFloat("Point Shadow Far Plane", &POINT_SHADOW_FAR_PLANE, 1.0f, 3000.0f);

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
	ImGui::SliderFloat("Camera Speed", &CAMERA_SPEED, 0.1f, 5.0f);
    ImGuiIO& io = ImGui::GetIO();
    for (std::shared_ptr<GameObject> obj : scene->objects) {
    ImGui::PushID(obj->name.c_str());

    if (ImGui::CollapsingHeader(obj->name.c_str())) {
        ImGui::Checkbox("Enabled", &obj->enabled);

        // Transform (always present)
        if (ImGui::TreeNode("Transform")) {
            ImGui::DragFloat3("Position", &obj->transform.position.x, 0.01f);
            ImGui::DragFloat3("Rotation", &obj->transform.rotation.x, 0.1f);
            ImGui::DragFloat3("Scale",    &obj->transform.scale.x, 0.01f);
            ImGui::TreePop();
        }

        // Light
        if (obj->light) {
            if (ImGui::TreeNode("Light")) {
                ImGui::ColorEdit3("Color", &obj->light->color.x);
                ImGui::DragFloat("Intensity", &obj->light->intensity, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat3("Direction", &obj->light->direction.x, 0.01f);
                ImGui::Checkbox("Casts Shadow", &obj->light->castsShadow);

                const char* types[] = { "Directional", "Spot", "Point" };
                int current = static_cast<int>(obj->light->type);
                if (ImGui::Combo("Type", &current, types, IM_ARRAYSIZE(types)))
                    obj->light->type = static_cast<LightType>(current);

                if (obj->light->type == LightType::Spot) {
                    float innerDeg = glm::degrees(glm::acos(obj->light->innerCutoff));
                    float outerDeg = glm::degrees(glm::acos(obj->light->outerCutoff));
                    if (ImGui::DragFloat("Inner Cutoff", &innerDeg, 0.1f, 0.0f, 90.0f))
                        obj->light->innerCutoff = glm::cos(glm::radians(innerDeg));
                    if (ImGui::DragFloat("Outer Cutoff", &outerDeg, 0.1f, 0.0f, 90.0f))
                        obj->light->outerCutoff = glm::cos(glm::radians(outerDeg));
                }
            	if (obj->light->type == LightType::Point) {
            		ImGui::DragFloat("Radius", &obj->light->radius, 0.1f, 0.1f, 100.0f);
            	}
                ImGui::TreePop();
            }
        }

        // Particle System
        if (obj->particleSystem) {
            if (ImGui::TreeNode("Particle System")) {
                auto& ps = obj->particleSystem;
                ImGui::DragFloat3("Position", &ps->position.x, 0.01f);
                ImGui::DragFloat3("Bounds Min", &ps->boundsMin.x, 0.1f);
                ImGui::DragFloat3("Bounds Max", &ps->boundsMax.x, 0.1f);
                ImGui::ColorEdit4("Start Color", &ps->startColor.x);
                ImGui::ColorEdit4("End Color", &ps->endColor.x);
                ImGui::DragFloat("Speed", &ps->speed, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Min Size", &ps->minSize, 0.01f, 0.001f, 50.0f);
                ImGui::DragFloat("Max Size", &ps->maxSize, 0.01f, 0.001f, 50.0f);
                ImGui::DragFloat("Min Lifetime", &ps->minLifetime, 0.1f, 0.1f, 30.0f);
                ImGui::DragFloat("Max Lifetime", &ps->maxLifetime, 0.1f, 0.1f, 30.0f);
                ImGui::DragFloat("Fade In Time", &ps->fadeInTime, 0.1f, 0.0f, 10.0f);
                ImGui::Text("Particles: %d", ps->GetCount());
                ImGui::TreePop();
            }
        }
		/*
        // Mesh
        if (obj->mesh) {
            if (ImGui::TreeNode("Mesh")) {
				obj->mesh->
            }
        }
        */
    }

    ImGui::PopID();
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

	std::shared_ptr<GameObject> light = std::make_shared<GameObject>("Light");
	light->light = std::make_shared<Light>();
	light->light->color = glm::vec3(1.0f, 0.0f, 0.0f);
	light->light->intensity = 1.0f;
	light->light->type = LightType::Spot;
	light->light->castsShadow = true;
	light->light->direction = glm::vec3(-1.0f, 0.0f, 0.0f);
	light->transform.position = glm::vec3(-6.0f, 0.25f, -0.015f);
	scene->Add(light);

	std::shared_ptr<GameObject> light2 = std::make_shared<GameObject>("Directional Light");
	light2->light = std::make_shared<Light>();
	//light2->rotationSpeed = 30.0f;
	light2->light->color = glm::vec3(1.0f, 0.0f, 0.0f);
	light2->light->intensity = 1.0f;
	light2->light->type = LightType::Directional;
	light2->light->castsShadow = true;
	light2->light->direction = glm::vec3(0.0f, -1.0f, 0.0f);
	light2->transform.position = glm::vec3(-6.0f, 2.25f, -0.015f);
	//scene->Add(light2);

	// Particles
	std::shared_ptr<GameObject> particles = std::make_shared<GameObject>("Particles");
	particles->particleSystem = std::make_shared<ParticleSystem>(particles->transform.position, 10000, true);
	particles->particleSystem->position = glm::vec3(-15.0f, 0.0f, -0.015f);
	particles->particleSystem->boundsMin = glm::vec3(-2.0f, 0.0f, -2.0f);
	particles->particleSystem->boundsMax = glm::vec3(15.0f, 2.5f,  2.0f);
	particles->particleSystem->minLifetime = 10.0f;
	particles->particleSystem->maxLifetime = 15.0f;
	particles->particleSystem->startColor = glm::vec4(0.8f, 0.8f, 0.8f, 0.9f);
	particles->particleSystem->endColor   = glm::vec4(0.8f, 0.8f, 0.8f, 0.0f);
	particles->particleSystem->speed      = 0.1f;
	particles->particleSystem->minSize    = 0.05f;
	particles->particleSystem->maxSize    = 0.1f;
	particleManager.Register(particles->particleSystem);
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
