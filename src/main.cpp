#include "config.h"
#include "scene.h"
#include "rendering/renderer.h"
#include "utils/profiler.h"
#include "rendering/effects/particles/particleSystem.h"
#include "rendering/effects/particles/particleSystemManager.h"
#include "utils/sceneLoader.h"
#include "rendering/debug/colliderDebugRenderer.h"
#include "physics/physicsSystem.h"
#include "../dependencies/imgui/imgui.h"
#include "../dependencies/imgui/imgui_impl_glfw.h"
#include "../dependencies/imgui/imgui_impl_opengl3.h"
#include "physics/rigidbodyComponent.h"
#include "rendering/effects/lights/lightComponent.h"
#include "rendering/mesh/meshComponent.h"
#include "rendering/effects/water/interactiveWaterComponent.h"
#include "physics/camera/characterControllerComponent.h"
#include "physics/camera/camera.h"

Camera* gCamera = nullptr;
float lastMouseX = 960.0f;
float lastMouseY = 540.0f;
float sensitivityX = 5.0f;
float sensitivityY = 5.0f;
bool firstMouse  = true;
bool cursorEnabled = false;
bool tabWasPressed = false;
bool fWasPressed = false;

extern bool DEBUG_COLLIDERS;
extern bool FREECAM_ENABLED;
extern std::string CURRENT_SCENE;
extern int GI_MODE;
extern float GI_INTENSITY;
extern bool DEBUG_PROBES;

Profiler profiler;
ParticleSystemManager particleManager;

std::shared_ptr<Scene> CreateScene();
void GUI(std::shared_ptr<Scene> scene, float deltaTime, Profiler& profiler, Renderer& renderer, PhysicsWorld& physics, std::shared_ptr<CharacterControllerComponent> controller);
void MouseCallback(GLFWwindow* window, double xpos, double ypos);
void ReloadScene(std::shared_ptr<Scene>& scene, Renderer& renderer, ParticleSystemManager& particleManager, PhysicsWorld& physics);
void DebugColliders(std::shared_ptr<Scene> scene, ColliderDebugRenderer& debugRenderer, Camera& camera, glm::mat4& projection);
void MovePlayerAndCamera(std::shared_ptr<Scene> scene, Camera& camera, GLFWwindow* window);

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

    Camera camera(glm::vec3(0, 1, 0));
	camera.yaw   = 0.0f;
	camera.pitch = 0.0f;
    gCamera = &camera;


    // Lock cursor7
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
	scene->probeGrid.Load();

	if (!scene->probeGrid.probes.empty()) {
		bool anyBaked = false;
		for (auto& p : scene->probeGrid.probes)
			if (p.baked) { anyBaked = true; break; }
		if (anyBaked)
			renderer.UploadProbeData(scene->probeGrid);
	}

    renderer.InitShadowMaps(scene);
	// Create collider debug renderer
	ColliderDebugRenderer debugRenderer;
	debugRenderer.Init();

	// Physics System
	PhysicsWorld physics;
	physics.Init();
	physics.RegisterScene(scene);

	// Create Player
	auto playerObj = std::make_shared<GameObject>("Player");
	playerObj->transform.position = glm::vec3(0, 0.56f, 0); // spawn position
	auto controller = playerObj->AddComponent<CharacterControllerComponent>();
	controller->Init(physics);
	controller->moveSpeed = 1.5f;
	playerObj->runtimeOnly = true;
	scene->Add(playerObj);

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

    	bool fPressed = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;

    	if (fPressed && !fWasPressed) {
    		FREECAM_ENABLED = !FREECAM_ENABLED;
    	}
    	fWasPressed = fPressed;

    	static bool gWasPressed = false;
    	bool gPressed = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
    	if (gPressed && !gWasPressed)
    		GI_MODE = (GI_MODE + 1) % 3; // cycles 0 -> 1 -> 2 -> 0
    	gWasPressed = gPressed;

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
    	{
    		if (FREECAM_ENABLED)
    		{
    			camera.ProcessKeyboard(window, deltaTime);
    		}
    		else
    		{
    			glm::vec3 forward = camera.transform.Forward();
    			glm::vec3 right   = camera.transform.Right();
    			forward.y = 0.0f;
    			right.y   = 0.0f;
    			if (glm::length(forward) > 0.001f) forward = glm::normalize(forward);
    			if (glm::length(right)   > 0.001f) right   = glm::normalize(right);

    			glm::vec3 moveDir(0.0f);
    			if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) moveDir += forward;
    			if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) moveDir -= forward;
    			if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) moveDir -= right;
    			if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) moveDir += right;
    			if (glm::length(moveDir) > 0.001f) moveDir = glm::normalize(moveDir);

    			bool jump = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    			controller->SetMoveInput(moveDir, jump);
    		}
    	}

        glfwPollEvents();
    	// Update scene
        scene->Update(deltaTime);
    	physics.Update(deltaTime);
    	physics.SyncTransforms(scene);

    	if (!FREECAM_ENABLED)
    	{
    		camera.transform.position = playerObj->transform.position
									  + glm::vec3(0, controller->eyeHeight, 0);
    	}

		// Render
        renderer.RenderFrame(camera, scene, profiler);
    	if (DEBUG_COLLIDERS) {
    		DebugColliders(scene, debugRenderer, camera, projection);
    	}

    	if (GI_MODE >= 1 && DEBUG_PROBES) {
    		auto& grid = scene->probeGrid;

    		// Draw the bounding box
    		glm::vec3 center = (grid.boundsMin + grid.boundsMax) * 0.5f;
    		glm::vec3 half = (grid.boundsMax - grid.boundsMin) * 0.5f;
    		debugRenderer.AddBox(center, glm::quat(1, 0, 0, 0), half, glm::vec3(1, 1, 0));

    		// Draw grid cell wireframes
    		glm::vec3 cellSize = (grid.boundsMax - grid.boundsMin) /
				glm::vec3(glm::max(grid.countX - 1, 1),
						  glm::max(grid.countY - 1, 1),
						  glm::max(grid.countZ - 1, 1));

    		glm::vec3 cellHalf = cellSize * 0.5f;

    		for (int z = 0; z < grid.countZ - 1; z++) {
    			for (int y = 0; y < grid.countY - 1; y++) {
    				for (int x = 0; x < grid.countX - 1; x++) {
    					glm::vec3 cellMin = grid.boundsMin + glm::vec3(x, y, z) * cellSize;
    					glm::vec3 cellCenter = cellMin + cellHalf;
    					debugRenderer.AddBox(cellCenter, glm::quat(1, 0, 0, 0), cellHalf,
											 glm::vec3(0.3f, 0.3f, 0.15f));
    				}
    			}
    		}

    		// Draw probes as spheres
    		for (const auto& probe : grid.probes) {
    			glm::vec3 color = probe.baked ? glm::vec3(0, 1, 0.5) : glm::vec3(0.5, 0.5, 0.5);
    			debugRenderer.AddSphere(probe.position, 0.08f, color);
    		}
    	}

        GUI(scene, deltaTime, profiler, renderer, physics, controller);
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
	physics.Shutdown();
    glfwTerminate();

    return 0;
}

void MovePlayerAndCamera(std::shared_ptr<Scene> scene, Camera& camera, GLFWwindow* window) {
	auto playerObj  = scene->GetObject("Player");
	auto controller = playerObj->GetComponent<CharacterControllerComponent>();

	if (!cursorEnabled && controller) {
		// Build world-space move direction from camera facing
		glm::vec3 forward = camera.transform.Forward();
		glm::vec3 right   = camera.transform.Right();
		forward.y = 0;  right.y = 0;  // flatten to XZ
		if (glm::length(forward) > 0) forward = glm::normalize(forward);
		if (glm::length(right)   > 0) right   = glm::normalize(right);

		glm::vec3 moveDir(0);
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) moveDir += forward;
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) moveDir -= forward;
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) moveDir -= right;
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) moveDir += right;
		if (glm::length(moveDir) > 0) moveDir = glm::normalize(moveDir);

		bool jump = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
		controller->SetMoveInput(moveDir, jump);
	}

	// After scene->Update() and physics.Update(), snap camera to player eye position:
	if (playerObj) {
		auto ctrl = playerObj->GetComponent<CharacterControllerComponent>();
		camera.transform.position = playerObj->transform.position
								  + glm::vec3(0, ctrl->eyeHeight, 0);
	}
}

void DebugColliders(std::shared_ptr<Scene> scene, ColliderDebugRenderer& debugRenderer, Camera& camera, glm::mat4& projection) {
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

void GUI(std::shared_ptr<Scene> scene, float deltaTime, Profiler& profiler, Renderer& renderer, PhysicsWorld& physics, std::shared_ptr<CharacterControllerComponent> controller)
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

	ImGui::Begin("Scene Settings");

	ImGui::SliderFloat("Ambient Light", &AMBIENT_MULTIPLIER, 0.0f, 1.0f);

	if (scene->skybox) {
		ImGui::Checkbox("Skybox", &SKYBOX_ENABLED);
	}

	if (GI_MODE >= 1) {
		ImGui::Checkbox("Debug Probe Grid", &DEBUG_PROBES);
		ImGui::SliderFloat("GI Intensity", &GI_INTENSITY, 0.0f, 3.0f);
		ImGui::Separator();
		ImGui::Text("Probe Grid");

		auto& grid = scene->probeGrid;

		bool gridChanged = false;
		gridChanged |= ImGui::DragFloat3("Grid Min", &grid.boundsMin.x, 0.1f);
		gridChanged |= ImGui::DragFloat3("Grid Max", &grid.boundsMax.x, 0.1f);
		gridChanged |= ImGui::DragInt("Probes X", &grid.countX, 1, 2, 32);
		gridChanged |= ImGui::DragInt("Probes Y", &grid.countY, 1, 2, 16);
		gridChanged |= ImGui::DragInt("Probes Z", &grid.countZ, 1, 2, 32);

		if (gridChanged)
			grid.Init();

		ImGui::Text("Total probes: %d", grid.TotalProbes());

		int bakedCount = 0;
		for (auto& p : grid.probes)
			if (p.baked) bakedCount++;
		ImGui::Text("Baked: %d / %d", bakedCount, grid.TotalProbes());

		if (GI_MODE == 1) {
			if (ImGui::Button("Bake (Rasterized)"))
				renderer.BakeLightProbes(scene);
		} else if (GI_MODE == 2) {
			ImGui::DragInt("Bounces", &PATH_TRACING_GI_BOUNCES, 1, 1, 5);
			ImGui::DragInt("Samples", &PATH_TRACING_GI_SAMPLES, 1, 1, 32);
			ImGui::DragInt("Face Size", &PATH_TRACING_GI_FACE_SIZE, 1, 1, 64);
			if (ImGui::Button("Bake (Path Traced)"))
				renderer.BakeLightProbes(scene);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Bake")) {
			grid.Init();
			renderer.ClearProbes();
		}
	}

	ImGui::End();


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

	ImGui::Begin("Debug");
	ImGui::Checkbox("Show Colliders", &DEBUG_COLLIDERS);
	ImGui::SliderFloat("Move Speed", &controller->moveSpeed, 0.1f, 5.0f);
	ImGui::Checkbox("Free Cam", &FREECAM_ENABLED);
	if (FREECAM_ENABLED)
		ImGui::SliderFloat("Camera Speed", &CAMERA_SPEED, 0.1f, 10.0f);
	ImGui::End();

    // Scene editor
    ImGui::Begin("Scene");
	if (ImGui::Button("Save Scene"))
		SceneLoader::Save(scene, "../assets/scenes/" + CURRENT_SCENE);
	ImGui::SameLine();
	if (ImGui::Button("Reload Scene"))
		ReloadScene(scene, renderer, particleManager, physics);
	ImGui::SameLine();
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
    	ImGui::Checkbox("Is Static", &obj->isStatic);

    	// Add Component
    	const char* componentTypes[] = { "Mesh", "Light", "Particle System", "Rigid Body", "Fog Volume", "Interactive Water"};
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
    			case 0: // Model
    				if (!obj->GetComponent<MeshComponent>()) {
    					auto model = std::make_shared<Model>();
    					model->Load("../assets/models/sphere.glb");
    					obj->AddComponent<MeshComponent>(model);
    				}
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
    			case 5: // Interactive Water
    				if (!obj->GetComponent<InteractiveWaterComponent>())
    					obj->AddComponent<InteractiveWaterComponent>();
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
		if (mc && mc->model) {
		    if (ImGui::TreeNode("Model")) {
		        static char modelPathBuf[256];
		        static int editingModelIndex = -1;

		        if (editingModelIndex != i) {
		            ImGui::Text("Path: %s", mc->model->sourcePath.c_str());
		            ImGui::Text("Sub-meshes: %d", (int)mc->model->subMeshes.size());
		            ImGui::Text("Materials: %d", (int)mc->model->materials.size());

		            if (ImGui::Button("Change Model")) {
		                editingModelIndex = i;
		                strncpy(modelPathBuf, mc->model->sourcePath.c_str(), sizeof(modelPathBuf) - 1);
		                modelPathBuf[sizeof(modelPathBuf) - 1] = '\0';
		            }
		        } else {
		            ImGui::InputText("Model Path", modelPathBuf, sizeof(modelPathBuf));
		            if (ImGui::Button("Load")) {
		                auto newModel = std::make_shared<Model>();
		                if (newModel->Load(std::string(modelPathBuf))) {
		                    mc->model = newModel;
		                    renderer.AssignDefaultShader(scene);
		                    renderer.InitShadowMaps(scene);
		                }
		                editingModelIndex = -1;
		            }
		            ImGui::SameLine();
		            if (ImGui::Button("Cancel"))
		                editingModelIndex = -1;
		        }

		    	if (mc->model->materials.empty()) {
		    		if (ImGui::Button("Add Material")) {
		    			mc->model->materials.push_back(Model::DefaultMaterial());
		    			for (auto& sm : mc->model->subMeshes)
		    				if (sm.materialIndex < 0)
		    					sm.materialIndex = 0;
		    		}
		    	} else {
		    		if (ImGui::Button("Add Material")) {
		    			mc->model->materials.push_back(Model::DefaultMaterial());
		    		}
		    	}

		        for (int m = 0; m < (int)mc->model->materials.size(); m++) {
		            auto& mat = mc->model->materials[m];
		            ImGui::PushID(m);
		            std::string label = "Material " + std::to_string(m);
		            if (ImGui::TreeNode(label.c_str())) {
		                // Texture path editing
		                static char albedoBuf[256];
		                static char normalBuf[256];
		                static char heightBuf[256];
		                static char mrBuf[256];
		                static int editingTexMat = -1;
		                int texEditId = i * 1000 + m;

		                if (editingTexMat != texEditId) {
		                    ImGui::Text("Albedo: %s", mat.texturePath.empty() ? "None" : mat.texturePath.c_str());
		                    ImGui::Text("Normal: %s", mat.hasNormalMap ? mat.normalMapPath.c_str() : "None");
		                    ImGui::Text("Height: %s", mat.hasHeightMap ? mat.heightMapPath.c_str() : "None");
		                    ImGui::Text("Met/Rough: %s", mat.hasMetallicRoughnessMap
		                        ? mat.metallicRoughnessMapPath.c_str() : "None");

		                    if (ImGui::Button("Edit Textures")) {
		                        editingTexMat = texEditId;
		                        strncpy(albedoBuf, mat.texturePath.c_str(), sizeof(albedoBuf) - 1);
		                        strncpy(normalBuf, mat.hasNormalMap ? mat.normalMapPath.c_str() : "", sizeof(normalBuf) - 1);
		                        strncpy(heightBuf, mat.hasHeightMap ? mat.heightMapPath.c_str() : "", sizeof(heightBuf) - 1);
		                        strncpy(mrBuf, mat.hasMetallicRoughnessMap ? mat.metallicRoughnessMapPath.c_str() : "", sizeof(mrBuf) - 1);
		                    }
		                } else {
		                    ImGui::InputText("Albedo", albedoBuf, sizeof(albedoBuf));
		                    ImGui::InputText("Normal Map", normalBuf, sizeof(normalBuf));
		                    ImGui::InputText("Height Map", heightBuf, sizeof(heightBuf));
		                    ImGui::InputText("Met/Rough Map", mrBuf, sizeof(mrBuf));

		                    if (ImGui::Button("Apply Textures")) {
		                        mc->model->ApplyTexturePaths(m,
		                            std::string(albedoBuf),
		                            std::string(normalBuf),
		                            std::string(heightBuf),
		                            std::string(mrBuf));
		                        editingTexMat = -1;
		                    }
		                    ImGui::SameLine();
		                    if (ImGui::Button("Cancel##tex"))
		                        editingTexMat = -1;
		                }

		                ImGui::Separator();
		                ImGui::DragFloat("Roughness", &mat.roughness, 0.01f, 0.05f, 1.0f);
		                ImGui::DragFloat("Metallic",  &mat.metallic,  0.01f, 0.0f, 1.0f);
		            	ImGui::Text("Emissive");
		            	ImGui::ColorEdit3("Emissive Color", &mat.emissiveColor.x);
		            	ImGui::DragFloat("Emissive Intensity", &mat.emissiveIntensity, 0.05f, 0.0f, 50.0f);
		                if (mat.hasHeightMap)
		                    ImGui::DragFloat("Height Scale", &mat.heightScale, 0.005f, 0.0f, 0.3f);

		                ImGui::TreePop();
		            }
		            ImGui::PopID();
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
    				rb->body->bodyID = JPH::BodyID();
    				physics.AddBody(obj);
    			}

    			ImGui::TreePop();
    		}
    	}

    	auto iw = obj->GetComponent<InteractiveWaterComponent>();
    	if (iw) {
    		if (ImGui::TreeNode("Interactive Water")) {
    			ImGui::DragInt("Resolution", &iw->resolution, 2, 64, 1024);
    			ImGui::DragFloat("Wave Speed", &iw->waveSpeed, 0.1f, 0.1f, 4.0f);
    			ImGui::DragFloat("Ripple Strength", &iw->rippleStrength, 0.1f, 0.1f, 1.0f);
    			ImGui::DragFloat("Fresnel Power", &iw->fresnelPower, 0.1f, 0.1f, 10.0f);

    			ImGui::DragFloat3("Shallow Color", &iw->shallowColor.x, 0.01f, 0.01f, 1.0f);
    			ImGui::DragFloat3("Deep Color", &iw->deepColor.x, 0.01f, 0.01f, 1.0f);

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
