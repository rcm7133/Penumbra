#pragma once
#include "../config.h"
#include "../scene.h"
#include "../gameobject.h"
#include "../physics/rigidbody.h"
#include "../particles/particleSystemManager.h"
#include "../../dependencies/nlohmann/json.hpp"
#include "../rendering/fogVolume.h"
#include "../components/particleSystemComponent.h"
#include "../components/rigidbodyComponent.h"
#include "../components/fogVolumeComponent.h"
#include "components/meshComponent.h"
#include "components/lightComponent.h"

class SceneLoader {
public:
    static std::shared_ptr<Scene> Load(const std::string& filePath, ParticleSystemManager& particleManager);
    static void Save(const std::shared_ptr<Scene>& scene, const std::string& filePath);

    static void SavePrefab(const std::shared_ptr<GameObject>& obj, const std::string& directory = "../assets/prefabs/");
    static std::shared_ptr<GameObject> LoadPrefab(const std::string& filepath, ParticleSystemManager& particleManager);
    static std::vector<std::string> ListPrefabs(const std::string& directory = "../assets/prefabs/");

private:
    using json = nlohmann::json;

    // Serialization Helper Functions
    static json SerializeVec3(const glm::vec3& v);
    static json SerializeVec4(const glm::vec4& v);
    static json SerializeTransform(const Transform& t);
    static json SerializeMesh(const Mesh& m);
    static json SerializeLight(const Light& l);
    static json SerializeParticleSystem(const ParticleSystem& ps);
    static json SerializeGameObject(const std::shared_ptr<GameObject>& obj);
    static json SerializeRigidBody(const RigidBody& rb);
    static json SerializeFogVolume(const FogVolume& fv);
    static json SerializeComponents(const std::shared_ptr<GameObject>& obj);

    static glm::vec3 DeserializeVec3(const json& j);
    static glm::vec4 DeserializeVec4(const json& j);
    static void DeserializeTransform(const json& j, Transform& t);
    static std::shared_ptr<Mesh> DeserializeMesh(const json& j);
    static std::shared_ptr<Light> DeserializeLight(const json& j);
    static std::shared_ptr<ParticleSystem> DeserializeParticleSystem(const json& j, const glm::vec3& ownerPos, ParticleSystemManager& particleManager);
    static std::shared_ptr<GameObject> DeserializeGameObject(const json& j, ParticleSystemManager& particleManager);
    static void DeserializeComponents(const json& j, std::shared_ptr<GameObject>& obj, ParticleSystemManager& particleManager);
    static std::shared_ptr<RigidBody> DeserializeRigidBody(const json& j);
    static std::shared_ptr<FogVolume> DeserializeFogVolume(const json& j);
};