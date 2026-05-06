#pragma once
#include "../../../config.h"
#include "../../../component.h"

class GameObject;

class ScriptComponent : public Component
{
public:
    GameObject* gameObject = nullptr;

    virtual void Start() override {}
    virtual void Update(float dt) override {}
    virtual void OnCollision(GameObject* other) {}
    virtual void OnDestroy() {}

    Transform& transform() { return gameObject->transform; }

    const char* GetTypeName() const override { return "ScriptComponent"; }
}