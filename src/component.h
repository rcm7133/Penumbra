#pragma once
#include "config.h"

#include "gameobject.h"

class GameObject;

class Component {
public:
    GameObject* owner = nullptr;
    bool enabled = true;

    virtual ~Component() = default;
    virtual void Start() {}
    virtual void Update(float deltaTime) {}
    virtual const char* GetTypeName() const = 0;
};