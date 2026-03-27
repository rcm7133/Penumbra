#include "config.h"
#include "mesh.h"
#include "scene.h"
#include "camera.h"
#include "gbuffer.h"
#include "quad.h"
#include "../dependencies/imgui/imgui.h"
#include "../dependencies/imgui/imgui_impl_glfw.h"
#include "../dependencies/imgui/imgui_impl_opengl3.h"

constexpr int MAX_SHADOW_LIGHTS = 2;
int SHADOW_WIDTH = 1024;
int SHADOW_HEIGHT = 1024;

Camera* gCamera = nullptr;
float lastMouseX = 960.0f;
float lastMouseY = 540.0f;
bool firstMouse  = true;
bool cursorEnabled = false;

void MouseCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse) {
        lastMouseX = xpos;
        lastMouseY = ypos;
        firstMouse = false;
    }
    float xOffset =  (xpos - lastMouseX);
    float yOffset =  (lastMouseY - ypos);
    lastMouseX = xpos;
    lastMouseY = ypos;
    if (!cursorEnabled)
        gCamera->ProcessMouse(xOffset, yOffset);
}


// Forward declarations
unsigned int MakeShaderModule(const std::string& filePath, unsigned int moduleType);
unsigned int MakeShaderProgram(const std::string& vertexFilepath, const std::string& fragmentFilepath);
std::shared_ptr<Scene> CreateScene();
void GUI(std::shared_ptr<Scene> scene);
void RenderShadowMap(unsigned int shader, const glm::mat4& lightSpaceMatrix, std::shared_ptr<Scene> scene);

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

    unsigned int gBufferShader  = MakeShaderProgram("../shaders/geometryVert.glsl",  "../shaders/geometryFrag.glsl");
    unsigned int lightingShader = MakeShaderProgram("../shaders/lightingVert.glsl", "../shaders/lightingFrag.glsl");
    unsigned int shadowShader = MakeShaderProgram("../shaders/shadowVert.glsl", "../shaders/shadowFrag.glsl");

    // Cache shadow shader locs
    int shadow_modelLoc = glGetUniformLocation(shadowShader, "model");
    int shadow_lightSpaceLoc = glGetUniformLocation(shadowShader, "lightSpaceMatrix");

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
            SHADOW_WIDTH, SHADOW_HEIGHT,
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
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS)
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
        if (!cursorEnabled)
            camera.ProcessKeyboard(window, deltaTime);
        glm::mat4 view = camera.GetViewMatrix();

        glfwPollEvents();
        scene->Update(deltaTime);

        // -----------------------------------------------
        // REALTIME SHADOWS
        // -----------------------------------------------
        int shadowIndex = 0;

        glUseProgram(shadowShader);

        for (auto& obj : scene->objects) {
            if (!obj->light || !obj->light->castsShadow)
                continue;

            if (shadowIndex >= MAX_SHADOW_LIGHTS)
                break;

            auto& light = obj->light;

            glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
            glBindFramebuffer(GL_FRAMEBUFFER, light->shadowFBO);
            glClear(GL_DEPTH_BUFFER_BIT);

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);

            glm::vec3 lightPos = obj->transform.position;
            glm::vec3 target   = glm::vec3(0.0f, 0.0f, 0.0f);
            glm::vec3 up       = glm::vec3(0.0f, 1.0f, 0.0f);
            glm::mat4 lightView = glm::lookAt(lightPos, target, up);

            glBindFramebuffer(GL_FRAMEBUFFER, light->shadowFBO);
            glViewport(0,0,SHADOW_WIDTH,SHADOW_HEIGHT);
            glClear(GL_DEPTH_BUFFER_BIT);

            RenderShadowMap(shadowShader, lightView, scene);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }


        // -----------------------------------------------
        // GEOMETRY PASS — render scene into G-Buffer
        // -----------------------------------------------
        gbuffer.Bind();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glUseProgram(gBufferShader);
        glUniformMatrix4fv(gBuf_view, 1, GL_FALSE, glm::value_ptr(view));
        scene->RenderGeometry(gBufferShader, gBuf_model, gBuf_shininess);
        gbuffer.Unbind();

        // -----------------------------------------------
        // LIGHTING PASS — fullscreen quad reads G-Buffer
        // -----------------------------------------------
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(lightingShader);

        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gbuffer.gPosition);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gbuffer.gNormal);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, gbuffer.gDiffuse);

        glUniform3f(light_cameraPos,
            camera.transform.position.x,
            camera.transform.position.y,
            camera.transform.position.z);

        scene->UploadLights(lightingShader);

        quad.Draw();

        GUI(scene);

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

void GUI(std::shared_ptr<Scene> scene)
{
    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();

    ImGui::Begin("Scene");
    ImGui::Text("Hello");
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
    wall->mesh = std::make_shared<Mesh>("../models/walls.obj", "../models/scene.png", 32.0);
    scene->Add(wall);

    std::shared_ptr<GameObject> table = std::make_shared<GameObject>("Table");
    table->mesh = std::make_shared<Mesh>("../models/table.obj", "../models/scene.png", 512.0);
    scene->Add(table);

    std::shared_ptr<GameObject> floor = std::make_shared<GameObject>("Floor");
    floor->mesh = std::make_shared<Mesh>("../models/floor.obj", "../models/scene.png", 512.0);
    scene->Add(floor);

    std::shared_ptr<GameObject> chairTops = std::make_shared<GameObject>("Chair Tops");
    chairTops->mesh = std::make_shared<Mesh>("../models/chairTops.obj", "../models/scene.png", 12.0);
    scene->Add(chairTops);

    std::shared_ptr<GameObject> chairBottoms = std::make_shared<GameObject>("Chair Bottoms");
    chairBottoms->mesh = std::make_shared<Mesh>("../models/chairBottoms.obj", "../models/scene.png", 64.0);
    scene->Add(chairBottoms);

    std::shared_ptr<Orbiter> orb = std::make_shared<Orbiter>("Orbiter");
    orb->light = std::make_shared<Light>();
    orb->light->color = glm::vec3(1.0f, 1.0f, 1.0f);
    orb->orbitCenter = glm::vec3(0.0f, 2.25f, 0.0f);
    orb->light->intensity = 0.5f;
    orb->orbitRadius = 1.0f;
    orb->orbitSpeed = 45.0f;
    scene->Add(orb);

    std::shared_ptr<Orbiter> orb2 = std::make_shared<Orbiter>("Orbiter 2");
    orb2->light = std::make_shared<Light>();
    orb2->light->color  = glm::vec3(1.0f, 1.0f, 1.0f);
    orb2->orbitCenter  = glm::vec3(0.0f, 2.5f, 0.0f);
    orb2->light->intensity = 0.5f;
    orb2->orbitRadius   = 0.75f;
    orb2->orbitSpeed    = 25.0f;
    scene->Add(orb2);

    std::shared_ptr<Orbiter> orb3 = std::make_shared<Orbiter>("Orbiter 3");
    orb3->light = std::make_shared<Light>();
    orb3->orbitCenter  = glm::vec3(0.0f, 2.5f, 0.0f);
    orb3->light->color  = glm::vec3(1.0f, 1.0f, 1.0f);
    orb3->light->intensity = 0.5f;
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
    while (std::getline(file, line)) {
        ss << line << "\n";
    }

    // Dump string stream into a string and converto to C style char array
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