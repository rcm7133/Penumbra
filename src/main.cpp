#include "config.h"
#include "rendering/mesh.h"
#include "scene.h"
#include "rendering/camera.h"
#include "rendering/renderer.h"
#include "profiler.h"
#include "particles/particleSystem.h"
#include "particles/particleSystemManager.h"
#include "utils/sceneLoader.h"
#include "rendering/colliderDebugRenderer.h"
#include "physics/physicsSystem.h"
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

extern bool DEBUG_COLLIDERS;

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
	std::shared_ptr<Scene> scene = SceneLoader::Load("../assets/scenes/tunnel.scene", particleManager);
    renderer.AssignDefaultShader(scene);
    scene->Start();
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
    	// Update scene
        scene->Update(deltaTime);
    	physics.Update(deltaTime);
    	physics.SyncTransforms(scene);
    	particleManager.Update(deltaTime);
		// Render
        renderer.RenderFrame(camera, scene, profiler);
    	if (DEBUG_COLLIDERS)
    	{
    		glm::vec3 green(0, 1, 0);
    		for (const auto& obj : scene->objects)
    		{
    			if (!obj->rigidBody) continue;
    			auto& rb = obj->rigidBody;
    			auto& t = obj->transform;

    			switch (rb->shapeType)
    			{
    				case RigidBody::Box:
    					debugRenderer.AddBox(t.position, t.rotation, rb->halfExtent, green);
    					break;
    				case RigidBody::Sphere:
    					debugRenderer.AddSphere(t.position, rb->radius, green);
    					break;
    				case RigidBody::Capsule:
    					debugRenderer.AddCapsule(t.position, t.rotation, rb->capsuleHalfHeight, rb->radius, green);
    					break;
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
	if (ImGui::Button("Save Scene"))
		SceneLoader::Save(scene, "../assets/scenes/tunnel.scene");
	if (ImGui::Button("Reload Scene"))
		ReloadScene(scene, renderer, particleManager, physics);
	ImGui::Checkbox("Show Colliders", &DEBUG_COLLIDERS);
    ImGuiIO& io = ImGui::GetIO();
    for (std::shared_ptr<GameObject> obj : scene->objects) {
    ImGui::PushID(obj->name.c_str());

    if (ImGui::CollapsingHeader(obj->name.c_str())) {
        ImGui::Checkbox("Enabled", &obj->enabled);

        // Transform (always present)
    	if (ImGui::TreeNode("Transform")) {
    		ImGui::DragFloat3("Position", &obj->transform.position.x, 0.01f);

    		glm::vec3 euler = obj->transform.GetEulerDegrees();
    		if (ImGui::DragFloat3("Rotation", &euler.x, 0.1f))
    			obj->transform.SetEulerDegrees(euler);

    		ImGui::DragFloat3("Scale", &obj->transform.scale.x, 0.01f);
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

    	if (obj->rigidBody) {
    		if (ImGui::TreeNode("Rigid Body")) {
    			auto& rb = obj->rigidBody;

    			const char* motions[] = { "Static", "Dynamic", "Kinematic" };
    			int curMotion = static_cast<int>(rb->motion);
    			if (ImGui::Combo("Motion", &curMotion, motions, IM_ARRAYSIZE(motions)))
    				rb->motion = static_cast<BodyMotion>(curMotion);

    			const char* shapes[] = { "Box", "Sphere", "Mesh", "Capsule" };
    			int curShape = static_cast<int>(rb->shapeType);
    			if (ImGui::Combo("Shape", &curShape, shapes, IM_ARRAYSIZE(shapes)))
    				rb->shapeType = static_cast<RigidBody::ShapeType>(curShape);

    			if (rb->shapeType == RigidBody::Box)
    				ImGui::DragFloat3("Half Extent", &rb->halfExtent.x, 0.01f, 0.01f, 100.0f);
    			if (rb->shapeType == RigidBody::Sphere || rb->shapeType == RigidBody::Capsule)
    				ImGui::DragFloat("Radius", &rb->radius, 0.01f, 0.01f, 50.0f);
    			if (rb->shapeType == RigidBody::Capsule)
    				ImGui::DragFloat("Half Height", &rb->capsuleHalfHeight, 0.01f, 0.01f, 50.0f);

    			ImGui::DragFloat("Mass", &rb->mass, 0.1f, 0.01f, 1000.0f);
    			ImGui::DragFloat("Friction", &rb->friction, 0.01f, 0.0f, 1.0f);
    			ImGui::DragFloat("Restitution", &rb->restitution, 0.01f, 0.0f, 1.0f);

    			if (ImGui::Button("Apply Changes")) {
    				physics.RemoveBody(rb);
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

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ReloadScene(std::shared_ptr<Scene>& scene, Renderer& renderer,
				 ParticleSystemManager& particleManager, PhysicsWorld& physics)
{
	particleManager.Reset();
	scene = SceneLoader::Load("../assets/scenes/tunnel.scene", particleManager);
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
