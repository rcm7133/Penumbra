#include "sceneLoader.h"

using json = nlohmann::json;

void SceneLoader::SavePrefab(const std::shared_ptr<GameObject>& obj, const std::string& directory) {
    json root = SerializeGameObject(obj);

    // Use the object's name as the filename, replacing spaces with underscores
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
    try {
        file >> root;
    } catch (const json::parse_error& e) {
        std::cerr << "SceneLoader::LoadPrefab - Parse error: " << e.what() << std::endl;
        return nullptr;
    }

    auto obj = DeserializeGameObject(root, particleManager);
    if (obj)
        std::cout << "Prefab loaded from " << filepath << std::endl;

    return obj;
}

std::vector<std::string> SceneLoader::ListPrefabs(const std::string& directory) {
    std::vector<std::string> prefabs;
    if (!std::filesystem::exists(directory)) return prefabs;

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".prefab")
            prefabs.push_back(entry.path().string());
    }
    std::sort(prefabs.begin(), prefabs.end());
    return prefabs;
}

json SceneLoader::SerializeVec3(const glm::vec3 &v) { return {v.x, v.y, v.z}; }

json SceneLoader::SerializeVec4(const glm::vec4 &v) { return {v.x, v.y, v.z, v.w}; }

json SceneLoader::SerializeTransform(const Transform& t) {
    return {
            { "position", SerializeVec3(t.position) },
            { "rotation", SerializeVec3(t.GetEulerDegrees()) },
            { "scale",    SerializeVec3(t.scale) }
    };
}

json SceneLoader::SerializeMesh(const Mesh &m) {
    json j;

    j["model"] = m.material.modelPath;
    j["texture"] = m.material.texturePath;
    j["shininess"] = m.material.shininess;
    if (m.material.hasNormalMap)
        j["normalMap"] = m.material.normalMapPath;
    return j;
}

json SceneLoader::SerializeLight(const Light &l) {
    json j;
    const char* typeNames[] = { "Directional", "Spot", "Point" };

    j["type"] = typeNames[static_cast<int>(l.type)];
    j["color"] = SerializeVec3(l.color);
    j["intensity"] = l.intensity;
    j["direction"] = SerializeVec3(l.direction);
    j["castsShadow"] = l.castsShadow;
    j["radius"] = l.radius;
    // Store cutopffs as degrees for readability
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

json SceneLoader::SerializeGameObject(const std::shared_ptr<GameObject>& obj) {
    json j;
    j["name"]      = obj->name;
    j["enabled"]   = obj->enabled;
    j["transform"] = SerializeTransform(obj->transform);

    // Determine subclass type
    if (dynamic_cast<RotatingLight*>(obj.get()))
    {
        auto* rl = static_cast<RotatingLight*>(obj.get());
        j["class"] = "RotatingLight";
        j["rotationSpeed"] = rl->rotationSpeed;
        j["rotationAngle"] = rl->rotationAngle;
    }
    else if (dynamic_cast<Orbiter*>(obj.get()))
    {
        auto* orb = static_cast<Orbiter*>(obj.get());
        j["class"]       = "Orbiter";
        j["orbitCenter"] = SerializeVec3(orb->orbitCenter);
        j["orbitRadius"] = orb->orbitRadius;
        j["orbitSpeed"]  = orb->orbitSpeed;
        j["orbitAngle"]  = orb->orbitAngle;
    }
    else
    {
        j["class"] = "GameObject";
    }

    if (obj->mesh)
        j["mesh"] = SerializeMesh(*obj->mesh);

    if (obj->light)
        j["light"] = SerializeLight(*obj->light);

    if (obj->particleSystem)
        j["particleSystem"] = SerializeParticleSystem(*obj->particleSystem);

    if (obj->rigidBody)
        j["rigidBody"] = SerializeRigidBody(*obj->rigidBody);

    if (obj->fogVolume)
        j["fogVolume"] = SerializeFogVolume(*obj->fogVolume);

    return j;
}

json SceneLoader::SerializeRigidBody(const RigidBody& rb) {
    json j;
    const char* motionNames[] = { "Static", "Dynamic", "Kinematic" };
    const char* shapeNames[]  = { "Box", "Sphere", "Mesh", "Capsule" };

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

void SceneLoader::Save(const std::shared_ptr<Scene>& scene, const std::string& filepath) {
    json root;

    root["objects"] = json::array();

    for (const auto& obj : scene->objects) {
        root["objects"].push_back(SerializeGameObject(obj));
    }

    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "SceneLoader::Save - Failed to open: " << filepath << std::endl;
        return;
    }

    file << root.dump(4); // pretty-print with 4-space indent
    file.close();
    std::cout << "Scene saved to " << filepath << std::endl;
}

// DESERIALIZATION

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
    std::string model      = j["model"].get<std::string>();
    std::string texture    = j["texture"].get<std::string>();
    float shininess        = j["shininess"].get<float>();
    std::string normalMap  = j.contains("normalMap") ? j["normalMap"].get<std::string>() : "";

    return std::make_shared<Mesh>(model, texture, shininess, normalMap);
}

std::shared_ptr<Light> SceneLoader::DeserializeLight(const json& j) {
    auto light = std::make_shared<Light>();

    std::string typeStr = j["type"].get<std::string>();
    if (typeStr == "Directional")  light->type = LightType::Directional;
    else if (typeStr == "Spot") light->type = LightType::Spot;
    else if (typeStr == "Point") light->type = LightType::Point;

    light->color = DeserializeVec3(j["color"]);
    light->intensity = j["intensity"].get<float>();
    light->direction = DeserializeVec3(j["direction"]);
    light->castsShadow = j["castsShadow"].get<bool>();

    if (j.contains("radius"))
        light->radius = j["radius"].get<float>();

    if (j.contains("innerCutoffDeg"))
        light->innerCutoff = glm::cos(glm::radians(j["innerCutoffDeg"].get<float>()));
    if (j.contains("outerCutoffDeg"))
        light->outerCutoff = glm::cos(glm::radians(j["outerCutoffDeg"].get<float>()));

    return light;
}

std::shared_ptr<ParticleSystem> SceneLoader::DeserializeParticleSystem(
    const json& j, const glm::vec3& ownerPos, ParticleSystemManager& particleManager)
{
    int maxParticles = j.value("maxParticles", 10000);
    bool lit = j.value("lit", true);

    auto ps = std::make_shared<ParticleSystem>(maxParticles, lit);

    ps->boundsMin = DeserializeVec3(j["boundsMin"]);
    ps->boundsMax = DeserializeVec3(j["boundsMax"]);
    ps->startColor = DeserializeVec4(j["startColor"]);
    ps->endColor = DeserializeVec4(j["endColor"]);
    ps->speed = j["speed"].get<float>();
    ps->minSize = j["minSize"].get<float>();
    ps->maxSize = j["maxSize"].get<float>();
    ps->minLifetime = j["minLifetime"].get<float>();
    ps->maxLifetime = j["maxLifetime"].get<float>();
    ps->fadeInTime = j.value("fadeInTime", 0.0f);

    particleManager.Register(ps);
    return ps;
}

std::shared_ptr<GameObject> SceneLoader::DeserializeGameObject(
    const json& j, ParticleSystemManager& particleManager)
{
    std::string cls  = j.value("class", "GameObject");
    std::string name = j["name"].get<std::string>();

    std::shared_ptr<GameObject> obj;

    if (cls == "RotatingLight") {
        auto rl = std::make_shared<RotatingLight>(name);
        rl->rotationSpeed = j.value("rotationSpeed", 90.0f);
        rl->rotationAngle = j.value("rotationAngle", 0.0f);
        obj = rl;
    }
    else if (cls == "Orbiter") {
        auto orb = std::make_shared<Orbiter>(name);
        if (j.contains("orbitCenter")) orb->orbitCenter = DeserializeVec3(j["orbitCenter"]);
        orb->orbitRadius = j.value("orbitRadius", 3.0f);
        orb->orbitSpeed  = j.value("orbitSpeed", 45.0f);
        orb->orbitAngle  = j.value("orbitAngle", 0.0f);
        obj = orb;
    }
    else {
        obj = std::make_shared<GameObject>(name);
    }

    obj->enabled = j.value("enabled", true);

    if (j.contains("transform"))
        DeserializeTransform(j["transform"], obj->transform);

    if (j.contains("mesh"))
        obj->mesh = DeserializeMesh(j["mesh"]);

    if (j.contains("light"))
        obj->light = DeserializeLight(j["light"]);

    if (j.contains("particleSystem"))
        obj->particleSystem = DeserializeParticleSystem(
            j["particleSystem"], obj->transform.position, particleManager);

    if (j.contains("rigidBody"))
        obj->rigidBody = DeserializeRigidBody(j["rigidBody"]);

    if (j.contains("fogVolume"))
        obj->fogVolume = DeserializeFogVolume(j["fogVolume"]);

    return obj;
}

std::shared_ptr<RigidBody> SceneLoader::DeserializeRigidBody(const json& j) {
    auto rb = std::make_shared<RigidBody>();

    std::string motionStr = j.value("motion", "Static");
    if (motionStr == "Dynamic")       rb->motion = BodyMotion::Dynamic;
    else if (motionStr == "Kinematic") rb->motion = BodyMotion::Kinematic;
    else                               rb->motion = BodyMotion::Static;

    std::string shapeStr = j.value("shape", "Box");
    if (shapeStr == "Sphere")       rb->shapeType = RigidBody::Sphere;
    else if (shapeStr == "Capsule") rb->shapeType = RigidBody::Capsule;
    else if (shapeStr == "Mesh")    rb->shapeType = RigidBody::Mesh;
    else                            rb->shapeType = RigidBody::Box;

    if (j.contains("halfExtent")) rb->halfExtent = DeserializeVec3(j["halfExtent"]);
    rb->radius            = j.value("radius", 0.5f);
    rb->capsuleHalfHeight = j.value("capsuleHalfHeight", 0.5f);
    rb->mass              = j.value("mass", 1.0f);
    rb->friction          = j.value("friction", 0.5f);
    rb->restitution       = j.value("restitution", 0.3f);
    return rb;
}

std::shared_ptr<FogVolume> SceneLoader::DeserializeFogVolume(const json& j) {
    auto fv = std::make_shared<FogVolume>();
    fv->boundsMin   = DeserializeVec3(j["boundsMin"]);
    fv->boundsMax   = DeserializeVec3(j["boundsMax"]);
    fv->density     = j.value("density", 0.5f);
    fv->scale       = j.value("scale", 0.5f);
    fv->scrollSpeed = j.value("scrollSpeed", 0.1f);
    return fv;
}

std::shared_ptr<Scene> SceneLoader::Load(const std::string& filepath, ParticleSystemManager& particleManager) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "SceneLoader::Load - Failed to open: " << filepath << std::endl;
        return nullptr;
    }

    json root;
    try {
        file >> root;
    } catch (const json::parse_error& e) {
        std::cerr << "SceneLoader::Load - Parse error: " << e.what() << std::endl;
        return nullptr;
    }

    auto scene = std::make_shared<Scene>();

    if (root.contains("objects")) {
        for (const auto& objJson : root["objects"]) {
            auto obj = DeserializeGameObject(objJson, particleManager);
            if (obj) scene->Add(obj);
        }
    }

    std::cout << "Scene loaded from " << filepath
              << " (" << scene->objects.size() << " objects)" << std::endl;
    return scene;
}

