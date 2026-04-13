#pragma once
#include "config.h"
#include "component.h"
#include "transform.h"
#include "physics/rigidbody.h"
#include "rendering/effects/water/interactiveWaterComponent.h"

class InteractiveWaterComponent;

class GameObject
{
public:
    Transform transform;
    bool enabled = true;
    bool runtimeOnly = false;
    std::string name = "GameObject";

    std::vector<std::shared_ptr<Component>> components;

    GameObject(const std::string& name = "GameObject") : name(name) {}

    template<typename T, typename... Args>
    std::shared_ptr<T> AddComponent(Args&&... args) {
        auto comp = std::make_shared<T>(std::forward<Args>(args)...);
        comp->owner = this;
        components.push_back(comp);
        return comp;
    }

    template<typename T>
    std::shared_ptr<T> GetComponent()
    {
        for (auto& comp : components)
        {
            auto casted = std::dynamic_pointer_cast<T>(comp);
            if (casted) return casted;
        }
        return nullptr;
    }

    template<typename T>
    bool HasComponent()
    {
        return GetComponent<T>() != nullptr;
    }

    void RemoveComponent(std::shared_ptr<Component> comp)
    {
        components.erase(
            std::remove(components.begin(), components.end(), comp),
            components.end());
    }

    virtual ~GameObject() {}

    bool IsForwardRendered() const {
        for (auto& comp : components) {
            if (auto wc = std::dynamic_pointer_cast<InteractiveWaterComponent>(comp))
                return true;
        }
        return false;
    }

    virtual void Update(float deltaTime)
    {
        for (auto& comp : components)
            if (comp->enabled) comp->Update(deltaTime);
    }

    virtual void Start()
    {
        for (auto& comp : components)
            comp->Start();
    }
};