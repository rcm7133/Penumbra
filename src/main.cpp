#include "config.h"
#include "scene.h"
#include "rendering/camera.h"
#include "rendering/renderer.h"
#include "../utils/profiler.h"
#include "particles/particleSystem.h"
#include "particles/particleSystemManager.h"
#include "utils/sceneLoader.h"
#include "rendering/colliderDebugRenderer.h"
#include "physics/physicsSystem.h"
#include "../dependencies/imgui/imgui.h"
#include "../dependencies/imgui/imgui_impl_glfw.h"
#include "../dependencies/imgui/imgui_impl_opengl3.h"
#include "components/rigidbodyComponent.h"
#include "components/lightComponent.h"
#include "components/meshComponent.h"

Camera* gCamera = nullptr;
float lastMouseX = 960.0f;
float lastMouseY = 540.0f;
float sensitivityX = 5.0f;
float sensitivityY = 5.0f;
bool firstMouse  = true;
bool cursorEnabled = false;
bool tabWasPressed = false;

extern bool DEBUG_COLLIDERS;
extern std::string CURRENT_SCENE;

Profiler profiler;
ParticleSystemManager particleManager;

std::shared_ptr<Scene> CreateScene();
void GUI(std::shared_ptr<Scene> scene, float deltaTime, Profiler& profiler, Renderer& renderer, PhysicsWorld& physics);
void MouseCallback(GLFWwindow* window, double xpos, double ypos);
void ReloadScene(std::shared_ptr<Scene>& scene, Renderer& renderer, ParticleSystemManager& particleManager, PhysicsWorld& physics);

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
	camera.yaw   = 250.0f;
	camera.pitch = 0.0f;
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
	std::shared_ptr<Scene> scene = SceneLoader::Load("../assets/scenes/" + CURRENT_SCENE, particleManager);
    renderer.AssignDefaultShader(scene);
	scene->Start();
	scene->LoadSkybox("../assets/textures/skybox");

    renderer.InitShadowMaps(scene);
	// Create collider debug renderer
	ColliderDebugRenderer debugRenderer;
	debugRenderer.Init();

	// Physics System
	PhysicsWorld physics;
	physics.Init();
	physics.RegisterScene(scene);

    double lastTime = glfwGetTime();
    int frameCount = 0;
    float lastFrame = 0.0f;

	std::string title = "Penumbra";
	glfwSetWindowTitle(window, title.c_str());

    while (!glfwWindowShouldClose(window))
    {
        profiler.Collect();
        float currentTime = glfwGetTime();
        float deltaTime   = currentTime - lastFrame;
        lastFrame         = currentTime;
        frameCount++;

        if (currentTime - lastTime >= 1.0)
        {
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
    	// Update scene
        scene->Update(deltaTime);
    	physics.Update(deltaTime);
    	physics.SyncTransforms(scene);
		// Render
        renderer.RenderFrame(camera, scene, profiler);
    	if (DEBUG_COLLIDERS)
    	{
    		glm::vec3 green(0, 1, 0);
    		glm::vec3 blue(0, 0.5, 1);
    		for (const auto& obj : scene->objects)
    		{
    			auto rb = obj->GetComponent<RigidBodyComponent>();
    			if (rb) {
    				auto& t = obj->transform;
    				switch (rb->body->shapeType)
    				{
    					case RigidBody::Box:
    						debugRenderer.AddBox(t.position, t.rotation, rb->body->halfExtent, green);
    						break;
    					case RigidBody::Sphere:
    						debugRenderer.AddSphere(t.position, rb->body->radius, green);
    						break;
    					case RigidBody::Capsule:
    						debugRenderer.AddCapsule(t.position, t.rotation, rb->body->capsuleHalfHeight, rb->body->radius, green);
    						break;
    				}
    			}

    			auto fv = obj->GetComponent<FogVolumeComponent>();
    			if (fv) {
    				glm::vec3 offset = obj->transform.position;
    				glm::vec3 center = (fv->volume->boundsMin + fv->volume->boundsMax) * 0.5f + offset;
    				glm::vec3 half   = (fv->volume->boundsMax - fv->volume->boundsMin) * 0.5f;
    				debugRenderer.AddBox(center, glm::quat(1, 0, 0, 0), half, blue);
    			}

    			auto ps = obj->GetComponent<ParticleSystemComponent>();
    			if (ps) {
    				glm::vec3 offset = obj->transform.position;
    				glm::vec3 center = (ps->system->boundsMin + ps->system->boundsMax) * 0.5f + offset;
    				glm::vec3 half   = (ps->system->boundsMax - ps->system->boundsMin) * 0.5f;
    				debugRenderer.AddBox(center, glm::quat(1, 0, 0, 0), half, glm::vec3(1.0f, 0.5f, 0.0f));
    			}
    		}
    		debugRenderer.Render(camera.GetViewMatrix(), projection);
    	}

        GUI(scene, deltaTime, profiler, renderer, physics);
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
	physics.Shutdown();
    glfwTerminate();

    return 0;
}

void GUI(std::shared_ptr<Scene> scene, float deltaTime, Profiler& profiler, Renderer& renderer, PhysicsWorld& physics)
{
	static std::vector<std::shared_ptr<GameObject>> deleteQueue;

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
		ImGui::SliderInt("Steps", &FOG_STEPS, 4, 64);
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
	if (ImGui::Button("Save Scene"))
		SceneLoader::Save(scene, "../assets/scenes/" + CURRENT_SCENE);
	if (ImGui::Button("Reload Scene"))
		ReloadScene(scene, renderer, particleManager, physics);
	ImGui::Checkbox("Show Colliders", &DEBUG_COLLIDERS);

	if (ImGui::Button("Add Empty GameObject")) {
		auto obj = std::make_shared<GameObject>("New GameObject");
		scene->Add(obj);
	}
	ImGui::Separator();

	// Prefab spawner
	ImGui::Separator();
	static std::vector<std::string> prefabList = SceneLoader::ListPrefabs();
	if (ImGui::Button("Refresh"))
		prefabList = SceneLoader::ListPrefabs();
	ImGui::SameLine();
	static int selectedPrefab = -1;
	std::vector<std::string> prefabNames;
	for (const auto& p : prefabList)
		prefabNames.push_back(std::filesystem::path(p).stem().string());

	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80);
	if (ImGui::BeginCombo("##Prefabs", selectedPrefab >= 0 ? prefabNames[selectedPrefab].c_str() : "Add Prefab...")) {
		for (int i = 0; i < (int)prefabNames.size(); i++) {
			if (ImGui::Selectable(prefabNames[i].c_str(), selectedPrefab == i))
				selectedPrefab = i;
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Button("Add") && selectedPrefab >= 0 && selectedPrefab < (int)prefabList.size()) {
		auto obj = SceneLoader::LoadPrefab(prefabList[selectedPrefab], particleManager);
		if (obj) {
			scene->Add(obj);
			renderer.AssignDefaultShader(scene);
			renderer.InitShadowMaps(scene);
		}
	}
	ImGui::Separator();

    for (int i = 0; i < (int)scene->objects.size(); i++) {
        auto& obj = scene->objects[i];
        ImGui::PushID(i);

    if (ImGui::CollapsingHeader(obj->name.c_str())) {
        ImGui::Checkbox("Enabled", &obj->enabled);

    	// Add Component
    	const char* componentTypes[] = { "Mesh", "Light", "Particle System", "Rigid Body", "Fog Volume" };
    	static int selectedComponent = -1;
    	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80);
    	if (ImGui::BeginCombo("##AddComponent", "Add Component...")) {
    		for (int c = 0; c < IM_ARRAYSIZE(componentTypes); c++) {
    			if (ImGui::Selectable(componentTypes[c])) {
    				selectedComponent = c;
    			}
    		}
    		ImGui::EndCombo();
    	}

    	if (selectedComponent >= 0) {
    		switch (selectedComponent) {
    			case 0: // Mesh
    				if (!obj->GetComponent<MeshComponent>())
    					obj->AddComponent<MeshComponent>(
							std::make_shared<Mesh>("../assets/models/sphere.obj",
												   "../assets/textures/default.png", 32.0f, ""));
    				break;
    			case 1: // Light
    				if (!obj->GetComponent<LightComponent>())
    					obj->AddComponent<LightComponent>();
    				break;
    			case 2: // Particle System
    				if (!obj->GetComponent<ParticleSystemComponent>()) {
    					auto comp = obj->AddComponent<ParticleSystemComponent>(10000, true);
    					particleManager.Register(comp->system);
    				}
    				break;
    			case 3: // Rigid Body
    				if (!obj->GetComponent<RigidBodyComponent>())
    					obj->AddComponent<RigidBodyComponent>();
    				break;
    			case 4: // Fog Volume
    				if (!obj->GetComponent<FogVolumeComponent>())
    					obj->AddComponent<FogVolumeComponent>();
    				break;
    		}
    		selectedComponent = -1;
    	}

    	static int editingIndex = -1;
    	static char renameBuffer[128];
    	if (editingIndex != i) {
    		ImGui::Text("Name: %s", obj->name.c_str());
    		ImGui::SameLine();
    		if (ImGui::Button("Rename")) {
    			editingIndex = i;
    			strncpy(renameBuffer, obj->name.c_str(), sizeof(renameBuffer) - 1);
    			renameBuffer[sizeof(renameBuffer) - 1] = '\0';
    		}
    	} else {
    		ImGui::SetNextItemWidth(200);
    		ImGui::InputText("Name", renameBuffer, sizeof(renameBuffer));
    		ImGui::SameLine();
    		if (ImGui::Button("Apply")) {
    			obj->name = renameBuffer;
    			editingIndex = -1;
    		}
    		ImGui::SameLine();
    		if (ImGui::Button("Cancel"))
    			editingIndex = -1;
    	}

    	// Save as prefab
    	if (ImGui::Button("Save as Prefab")) {
    		SceneLoader::SavePrefab(obj);
    		prefabList = SceneLoader::ListPrefabs();
    	}

    	ImGui::SameLine();
    	if (ImGui::Button("Delete"))
    		deleteQueue.push_back(obj);

        // Transform (always present)
    	if (ImGui::TreeNode("Transform")) {
    		ImGui::DragFloat3("Position", &obj->transform.position.x, 0.01f);

    		glm::vec3 euler = obj->transform.GetEulerDegrees();
    		if (ImGui::DragFloat3("Rotation", &euler.x, 0.1f))
    			obj->transform.SetEulerDegrees(euler);

    		ImGui::DragFloat3("Scale", &obj->transform.scale.x, 0.01f);
    		ImGui::TreePop();
    	}

    	auto mc = obj->GetComponent<MeshComponent>();
	    if (mc) {
	        if (ImGui::TreeNode("Mesh")) {
	            static char modelBuf[256];
	            static char textureBuf[256];
	            static char normalBuf[256];
	            static int editingMeshIndex = -1;

	            if (editingMeshIndex != i) {
	                ImGui::Text("Model: %s", mc->mesh->material.modelPath.c_str());
	                ImGui::Text("Texture: %s", mc->mesh->material.texturePath.c_str());
	                ImGui::Text("Normal Map: %s", mc->mesh->material.hasNormalMap
	                    ? mc->mesh->material.normalMapPath.c_str() : "None");
	                ImGui::DragFloat("Shininess", &mc->mesh->material.shininess, 1.0f, 1.0f, 256.0f);

	                if (ImGui::Button("Edit Paths")) {
	                    editingMeshIndex = i;
	                    strncpy(modelBuf, mc->mesh->material.modelPath.c_str(), sizeof(modelBuf) - 1);
	                    strncpy(textureBuf, mc->mesh->material.texturePath.c_str(), sizeof(textureBuf) - 1);
	                    strncpy(normalBuf, mc->mesh->material.hasNormalMap
	                        ? mc->mesh->material.normalMapPath.c_str() : "", sizeof(normalBuf) - 1);
	                }
	            } else {
	                ImGui::InputText("Model", modelBuf, sizeof(modelBuf));
	                ImGui::InputText("Texture", textureBuf, sizeof(textureBuf));
	                ImGui::InputText("Normal Map", normalBuf, sizeof(normalBuf));
	                ImGui::DragFloat("Shininess", &mc->mesh->material.shininess, 1.0f, 1.0f, 256.0f);

	                if (ImGui::Button("Apply")) {
	                    float shininess = mc->mesh->material.shininess;
	                    std::string normalStr(normalBuf);
	                    mc->mesh = std::make_shared<Mesh>(
	                        std::string(modelBuf),
	                        std::string(textureBuf),
	                        shininess,
	                        normalStr
	                    );
	                    editingMeshIndex = -1;
	                }
	                ImGui::SameLine();
	                if (ImGui::Button("Cancel"))
	                    editingMeshIndex = -1;
	            }

	            ImGui::TreePop();
	        }
	    }

        // Light
    	auto lc = obj->GetComponent<LightComponent>();
    	if (lc) {
    		if (ImGui::TreeNode("Light")) {
    			ImGui::ColorEdit3("Color", &lc->light->color.x);
    			ImGui::DragFloat("Intensity", &lc->light->intensity, 0.01f, 0.0f, 10.0f);
    			ImGui::DragFloat3("Direction", &lc->light->direction.x, 0.01f);
    			ImGui::Checkbox("Casts Shadow", &lc->light->castsShadow);

    			const char* types[] = { "Directional", "Spot", "Point" };
    			int current = static_cast<int>(lc->light->type);
    			if (ImGui::Combo("Type", &current, types, IM_ARRAYSIZE(types)))
    				lc->light->type = static_cast<LightType>(current);

    			if (lc->light->type == LightType::Spot) {
    				float innerDeg = glm::degrees(glm::acos(lc->light->innerCutoff));
    				float outerDeg = glm::degrees(glm::acos(lc->light->outerCutoff));
    				if (ImGui::DragFloat("Inner Cutoff", &innerDeg, 0.1f, 0.0f, 90.0f))
    					lc->light->innerCutoff = glm::cos(glm::radians(innerDeg));
    				if (ImGui::DragFloat("Outer Cutoff", &outerDeg, 0.1f, 0.0f, 90.0f))
    					lc->light->outerCutoff = glm::cos(glm::radians(outerDeg));
    			}
    			if (lc->light->type == LightType::Point) {
    				ImGui::DragFloat("Radius", &lc->light->radius, 0.1f, 0.1f, 100.0f);
    			}
    			ImGui::TreePop();
    		}
    	}

    	auto ps = obj->GetComponent<ParticleSystemComponent>();
        // Particle System
        if (ps) {
            if (ImGui::TreeNode("Particle System")) {
                ImGui::DragFloat3("Bounds Min", &ps->system->boundsMin.x, 0.1f);
                ImGui::DragFloat3("Bounds Max", &ps->system->boundsMax.x, 0.1f);
                ImGui::ColorEdit4("Start Color", &ps->system->startColor.x);
                ImGui::ColorEdit4("End Color", &ps->system->endColor.x);
                ImGui::DragFloat("Speed", &ps->system->speed, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Min Size", &ps->system->minSize, 0.01f, 0.001f, 50.0f);
                ImGui::DragFloat("Max Size", &ps->system->maxSize, 0.01f, 0.001f, 50.0f);
                ImGui::DragFloat("Min Lifetime", &ps->system->minLifetime, 0.1f, 0.1f, 30.0f);
                ImGui::DragFloat("Max Lifetime", &ps->system->maxLifetime, 0.1f, 0.1f, 30.0f);
                ImGui::DragFloat("Fade In Time", &ps->system->fadeInTime, 0.1f, 0.0f, 10.0f);
                ImGui::Text("Particles: %d", ps->system->GetCount());
                ImGui::TreePop();
            }
        }

    	auto fv = obj->GetComponent<FogVolumeComponent>();
    	if (fv) {
    		if (ImGui::TreeNode("Fog Volume##component")) {
    			ImGui::DragFloat3("Bounds Min", &fv->volume->boundsMin.x, 0.1f);
    			ImGui::DragFloat3("Bounds Max", &fv->volume->boundsMax.x, 0.1f);
    			ImGui::DragFloat("Density", &fv->volume->density, 0.01f, 0.0f, 2.0f);
    			ImGui::DragFloat("Scale", &fv->volume->scale, 0.01f, 0.01f, 2.0f);
    			ImGui::DragFloat("Scroll Speed", &fv->volume->scrollSpeed, 0.01f, 0.0f, 2.0f);
    			ImGui::TreePop();
    		}
    	}

    	auto rb = obj->GetComponent<RigidBodyComponent>();
    	if (rb) {
    		if (ImGui::TreeNode("Rigid Body")) {
    			const char* motions[] = { "Static", "Dynamic", "Kinematic" };
    			int curMotion = static_cast<int>(rb->body->motion);
    			if (ImGui::Combo("Motion", &curMotion, motions, IM_ARRAYSIZE(motions)))
    				rb->body->motion = static_cast<BodyMotion>(curMotion);

    			const char* shapes[] = { "Box", "Sphere", "Mesh", "Capsule" };
    			int curShape = static_cast<int>(rb->body->shapeType);
    			if (ImGui::Combo("Shape", &curShape, shapes, IM_ARRAYSIZE(shapes)))
    				rb->body->shapeType = static_cast<RigidBody::ShapeType>(curShape);

    			if (rb->body->shapeType == RigidBody::Box)
    				ImGui::DragFloat3("Half Extent", &rb->body->halfExtent.x, 0.01f, 0.01f, 100.0f);
    			if (rb->body->shapeType == RigidBody::Sphere || rb->body->shapeType == RigidBody::Capsule)
    				ImGui::DragFloat("Radius", &rb->body->radius, 0.01f, 0.01f, 50.0f);
    			if (rb->body->shapeType == RigidBody::Capsule)
    				ImGui::DragFloat("Half Height", &rb->body->capsuleHalfHeight, 0.01f, 0.01f, 50.0f);

    			ImGui::DragFloat("Mass", &rb->body->mass, 0.1f, 0.01f, 1000.0f);
    			ImGui::DragFloat("Friction", &rb->body->friction, 0.01f, 0.0f, 1.0f);
    			ImGui::DragFloat("Restitution", &rb->body->restitution, 0.01f, 0.0f, 1.0f);

    			if (ImGui::Button("Apply Changes")) {
    				physics.RemoveBody(rb->body);
    				physics.AddBody(obj);
    			}

    			ImGui::TreePop();
    		}
    	}
    }

    ImGui::PopID();
}
    ImGui::Separator();
    ImGui::End();

	// Process deletions
	for (auto& obj : deleteQueue) {
		scene->objects.erase(
			std::remove(scene->objects.begin(), scene->objects.end(), obj),
			scene->objects.end());
	}
	deleteQueue.clear();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ReloadScene(std::shared_ptr<Scene>& scene, Renderer& renderer,
				 ParticleSystemManager& particleManager, PhysicsWorld& physics)
{
	particleManager.Reset();
	scene = SceneLoader::Load("../assets/scenes/" + CURRENT_SCENE, particleManager);
	renderer.AssignDefaultShader(scene);
	scene->Start();
	renderer.InitShadowMaps(scene);
	physics.RegisterScene(scene);
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
