#include "sceneLoader.h"
#include <filesystem>

using json = nlohmann::json;

// --- Prefabs ---

void SceneLoader::SavePrefab(const std::shared_ptr<GameObject>& obj, const std::string& directory) {
    std::filesystem::create_directories(directory);
    json root = SerializeGameObject(obj);
    std::string filename = obj->name;
    std::replace(filename.begin(), filename.end(), ' ', '_');
    std::string filepath = directory + filename + ".prefab";
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "SceneLoader::SavePrefab - Failed to open: " << filepath << std::endl;
        return;
    }
    file << root.dump(4);
    file.close();
    std::cout << "Prefab saved to " << filepath << std::endl;
}

std::shared_ptr<GameObject> SceneLoader::LoadPrefab(const std::string& filepath, ParticleSystemManager& particleManager) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "SceneLoader::LoadPrefab - Failed to open: " << filepath << std::endl;
        return nullptr;
    }
    json root;
    try { file >> root; }
    catch (const json::parse_error& e) {
        std::cerr << "SceneLoader::LoadPrefab - Parse error: " << e.what() << std::endl;
        return nullptr;
    }
    auto obj = DeserializeGameObject(root, particleManager);
    if (obj) std::cout << "Prefab loaded from " << filepath << std::endl;
    return obj;
}

std::vector<std::string> SceneLoader::ListPrefabs(const std::string& directory) {
    std::vector<std::string> prefabs;
    if (!std::filesystem::exists(directory)) return prefabs;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
        if (entry.path().extension() == ".prefab")
            prefabs.push_back(entry.path().string());
    std::sort(prefabs.begin(), prefabs.end());
    return prefabs;
}

// --- Serialization helpers ---

json SceneLoader::SerializeVec3(const glm::vec3& v) { return {v.x, v.y, v.z}; }
json SceneLoader::SerializeVec4(const glm::vec4& v) { return {v.x, v.y, v.z, v.w}; }

json SceneLoader::SerializeTransform(const Transform& t) {
    return {
        {"position", SerializeVec3(t.position)},
        {"rotation", SerializeVec3(t.GetEulerDegrees())},
        {"scale",    SerializeVec3(t.scale)}
    };
}

json SceneLoader::SerializeMesh(const Mesh& m) {
    json j;
    j["model"] = m.material.modelPath;
    j["texture"] = m.material.texturePath;
    j["shininess"] = m.material.shininess;
    if (m.material.hasNormalMap)
        j["normalMap"] = m.material.normalMapPath;
    return j;
}

json SceneLoader::SerializeLight(const Light& l) {
    json j;
    const char* typeNames[] = {"Directional", "Spot", "Point"};
    j["type"] = typeNames[static_cast<int>(l.type)];
    j["color"] = SerializeVec3(l.color);
    j["intensity"] = l.intensity;
    j["direction"] = SerializeVec3(l.direction);
    j["castsShadow"] = l.castsShadow;
    j["radius"] = l.radius;
    j["innerCutoffDeg"] = glm::degrees(glm::acos(l.innerCutoff));
    j["outerCutoffDeg"] = glm::degrees(glm::acos(l.outerCutoff));
    return j;
}

json SceneLoader::SerializeParticleSystem(const ParticleSystem& ps) {
    json j;
    j["maxParticles"] = ps.maxParticles;
    j["lit"]         = ps.IsLit();
    j["boundsMin"]   = SerializeVec3(ps.boundsMin);
    j["boundsMax"]   = SerializeVec3(ps.boundsMax);
    j["startColor"]  = SerializeVec4(ps.startColor);
    j["endColor"]    = SerializeVec4(ps.endColor);
    j["speed"]       = ps.speed;
    j["minSize"]     = ps.minSize;
    j["maxSize"]     = ps.maxSize;
    j["minLifetime"] = ps.minLifetime;
    j["maxLifetime"] = ps.maxLifetime;
    j["fadeInTime"]  = ps.fadeInTime;
    return j;
}

json SceneLoader::SerializeRigidBody(const RigidBody& rb) {
    json j;
    const char* motionNames[] = {"Static", "Dynamic", "Kinematic"};
    const char* shapeNames[]  = {"Box", "Sphere", "Mesh", "Capsule"};
    j["motion"]    = motionNames[static_cast<int>(rb.motion)];
    j["shape"]     = shapeNames[static_cast<int>(rb.shapeType)];
    j["halfExtent"] = SerializeVec3(rb.halfExtent);
    j["radius"]    = rb.radius;
    j["capsuleHalfHeight"] = rb.capsuleHalfHeight;
    j["mass"]      = rb.mass;
    j["friction"]  = rb.friction;
    j["restitution"] = rb.restitution;
    return j;
}

json SceneLoader::SerializeFogVolume(const FogVolume& fv) {
    json j;
    j["boundsMin"]   = SerializeVec3(fv.boundsMin);
    j["boundsMax"]   = SerializeVec3(fv.boundsMax);
    j["density"]     = fv.density;
    j["scale"]       = fv.scale;
    j["scrollSpeed"] = fv.scrollSpeed;
    return j;
}

json SceneLoader::SerializeComponents(const std::shared_ptr<GameObject>& obj) {
    json comps = json::array();

    for (auto& comp : obj->components) {
        json c;

        if (auto mc = std::dynamic_pointer_cast<MeshComponent>(comp)) {
            c["type"] = "Mesh";
            c["data"] = SerializeMesh(*mc->mesh);
        }
        else if (auto lc = std::dynamic_pointer_cast<LightComponent>(comp)) {
            c["type"] = "Light";
            c["data"] = SerializeLight(*lc->light);
        }
        else if (auto ps = std::dynamic_pointer_cast<ParticleSystemComponent>(comp)) {
            c["type"] = "ParticleSystem";
            c["data"] = SerializeParticleSystem(*ps->system);
        }
        else if (auto rb = std::dynamic_pointer_cast<RigidBodyComponent>(comp)) {
            c["type"] = "RigidBody";
            c["data"] = SerializeRigidBody(*rb->body);
        }
        else if (auto fv = std::dynamic_pointer_cast<FogVolumeComponent>(comp)) {
            c["type"] = "FogVolume";
            c["data"] = SerializeFogVolume(*fv->volume);
        }

        if (!c.empty())
            comps.push_back(c);
    }

    return comps;
}


json SceneLoader::SerializeGameObject(const std::shared_ptr<GameObject>& obj) {
    json j;
    j["name"]      = obj->name;
    j["enabled"]   = obj->enabled;
    j["transform"] = SerializeTransform(obj->transform);
    j["class"]     = "GameObject";

    json comps = SerializeComponents(obj);
    if (!comps.empty())
        j["components"] = comps;

    return j;
}

// --- Scene save ---

void SceneLoader::Save(const std::shared_ptr<Scene>& scene, const std::string& filepath) {
    json root;
    root["objects"] = json::array();
    for (const auto& obj : scene->objects)
        root["objects"].push_back(SerializeGameObject(obj));
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "SceneLoader::Save - Failed to open: " << filepath << std::endl;
        return;
    }
    file << root.dump(4);
    file.close();
    std::cout << "Scene saved to " << filepath << std::endl;
}

// --- Deserialization ---

glm::vec3 SceneLoader::DeserializeVec3(const json& j) {
    return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

glm::vec4 SceneLoader::DeserializeVec4(const json& j) {
    return glm::vec4(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
}

void SceneLoader::DeserializeTransform(const json& j, Transform& t) {
    t.position = DeserializeVec3(j["position"]);
    t.SetEulerDegrees(DeserializeVec3(j["rotation"]));
    t.scale = DeserializeVec3(j["scale"]);
}

std::shared_ptr<Mesh> SceneLoader::DeserializeMesh(const json& j) {
    std::string model     = j["model"].get<std::string>();
    std::string texture   = j["texture"].get<std::string>();
    float shininess       = j["shininess"].get<float>();
    std::string normalMap = j.contains("normalMap") ? j["normalMap"].get<std::string>() : "";
    return std::make_shared<Mesh>(model, texture, shininess, normalMap);
}

std::shared_ptr<Light> SceneLoader::DeserializeLight(const json& j) {
    auto light = std::make_shared<Light>();
    std::string typeStr = j["type"].get<std::string>();
    if (typeStr == "Directional")  light->type = LightType::Directional;
    else if (typeStr == "Spot")    light->type = LightType::Spot;
    else if (typeStr == "Point")   light->type = LightType::Point;
    light->color = DeserializeVec3(j["color"]);
    light->intensity = j["intensity"].get<float>();
    light->direction = DeserializeVec3(j["direction"]);
    light->castsShadow = j["castsShadow"].get<bool>();
    if (j.contains("radius"))        light->radius = j["radius"].get<float>();
    if (j.contains("innerCutoffDeg")) light->innerCutoff = glm::cos(glm::radians(j["innerCutoffDeg"].get<float>()));
    if (j.contains("outerCutoffDeg")) light->outerCutoff = glm::cos(glm::radians(j["outerCutoffDeg"].get<float>()));
    return light;
}

void SceneLoader::DeserializeComponents(const json& j, std::shared_ptr<GameObject>& obj, ParticleSystemManager& particleManager) {
    for (const auto& compJson : j) {
        std::string type = compJson["type"].get<std::string>();
        const json& data = compJson["data"];

        if (type == "Mesh") {
            auto mesh = DeserializeMesh(data);
            obj->AddComponent<MeshComponent>(mesh);
        }
        else if (type == "Light") {
            auto light = DeserializeLight(data);
            obj->AddComponent<LightComponent>(light);
        }
        else if (type == "ParticleSystem") {
            int maxParticles = data.value("maxParticles", 10000);
            bool lit = data.value("lit", true);
            auto comp = obj->AddComponent<ParticleSystemComponent>(maxParticles, lit);
            auto& ps = comp->system;
            ps->boundsMin   = DeserializeVec3(data["boundsMin"]);
            ps->boundsMax   = DeserializeVec3(data["boundsMax"]);
            ps->startColor  = DeserializeVec4(data["startColor"]);
            ps->endColor    = DeserializeVec4(data["endColor"]);
            ps->speed       = data["speed"].get<float>();
            ps->minSize     = data["minSize"].get<float>();
            ps->maxSize     = data["maxSize"].get<float>();
            ps->minLifetime = data["minLifetime"].get<float>();
            ps->maxLifetime = data["maxLifetime"].get<float>();
            ps->fadeInTime  = data.value("fadeInTime", 0.0f);
            particleManager.Register(ps);
        }
        else if (type == "RigidBody") {
            auto comp = obj->AddComponent<RigidBodyComponent>();
            auto& rb = comp->body;
            std::string motionStr = data.value("motion", "Static");
            if (motionStr == "Dynamic")        rb->motion = BodyMotion::Dynamic;
            else if (motionStr == "Kinematic")  rb->motion = BodyMotion::Kinematic;
            else                                rb->motion = BodyMotion::Static;
            std::string shapeStr = data.value("shape", "Box");
            if (shapeStr == "Sphere")       rb->shapeType = RigidBody::Sphere;
            else if (shapeStr == "Capsule") rb->shapeType = RigidBody::Capsule;
            else if (shapeStr == "Mesh")    rb->shapeType = RigidBody::Mesh;
            else                            rb->shapeType = RigidBody::Box;
            if (data.contains("halfExtent")) rb->halfExtent = DeserializeVec3(data["halfExtent"]);
            rb->radius            = data.value("radius", 0.5f);
            rb->capsuleHalfHeight = data.value("capsuleHalfHeight", 0.5f);
            rb->mass              = data.value("mass", 1.0f);
            rb->friction          = data.value("friction", 0.5f);
            rb->restitution       = data.value("restitution", 0.3f);
        }
        else if (type == "FogVolume") {
            auto comp = obj->AddComponent<FogVolumeComponent>();
            auto& fv = comp->volume;
            fv->boundsMin   = DeserializeVec3(data["boundsMin"]);
            fv->boundsMax   = DeserializeVec3(data["boundsMax"]);
            fv->density     = data.value("density", 0.5f);
            fv->scale       = data.value("scale", 0.5f);
            fv->scrollSpeed = data.value("scrollSpeed", 0.1f);
        }
    }
}

std::shared_ptr<GameObject> SceneLoader::DeserializeGameObject(const json& j, ParticleSystemManager& particleManager) {
    std::string name = j["name"].get<std::string>();
    auto obj = std::make_shared<GameObject>(name);
    obj->enabled = j.value("enabled", true);

    if (j.contains("transform"))
        DeserializeTransform(j["transform"], obj->transform);

    if (j.contains("components"))
        DeserializeComponents(j["components"], obj, particleManager);

    return obj;
}

// --- Scene load ---

std::shared_ptr<Scene> SceneLoader::Load(const std::string& filepath, ParticleSystemManager& particleManager) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "SceneLoader::Load - Failed to open: " << filepath << std::endl;
        return nullptr;
    }
    json root;
    try { file >> root; }
    catch (const json::parse_error& e) {
        std::cerr << "SceneLoader::Load - Parse error: " << e.what() << std::endl;
        return nullptr;
    }
    auto scene = std::make_shared<Scene>();
    if (root.contains("objects"))
        for (const auto& objJson : root["objects"])
            if (auto obj = DeserializeGameObject(objJson, particleManager))
                scene->Add(obj);
    std::cout << "Scene loaded from " << filepath
              << " (" << scene->objects.size() << " objects)" << std::endl;
    return scene;
}