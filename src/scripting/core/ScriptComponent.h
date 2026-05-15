#pragma once
#include "../../config.h"
#include "../../component.h"
#include "../../../dependencies/nlohmann/json.hpp"

class GameObject;

class ScriptComponent : public Component
{
public:
    std::string scriptName = "ScriptComponent";

    virtual void Start() override {}
    virtual void Update(float dt) override {}
    virtual void OnCollision(GameObject* other) {}
    virtual void OnDestroy() {}

    virtual nlohmann::json  Serialize() const { return {}; }
    virtual void Deserialize(const nlohmann::json&) {}

    Transform& transform() { return owner->transform; }

    const char* GetTypeName() const override { return "ScriptComponent"; }
    virtual const char* GetScriptName() const { return scriptName.c_str(); }
};