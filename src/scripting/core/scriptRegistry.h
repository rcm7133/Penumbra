#pragma once
#include "ScriptComponent.h"
#include "../../config.h"

class ScriptRegistry {
public:
    using Factory = std::function<std::shared_ptr<ScriptComponent>()>;

    static ScriptRegistry& Get() {
        static ScriptRegistry instance;
        return instance;
    }

    void Register(const std::string& name, Factory factory) {
        factories[name] = std::move(factory);
    }

    std::shared_ptr<ScriptComponent> Create(const std::string& name) const {
        auto it = factories.find(name);
        if (it == factories.end()) return nullptr;
        return it->second();
    }

    const std::unordered_map<std::string, Factory>& GetAll() const {
        return factories;
    }

private:
    std::unordered_map<std::string, Factory> factories;
};

#define REGISTER_SCRIPT(ClassName) \
    static bool _registered_##ClassName = []() { \
        ScriptRegistry::Get().Register(#ClassName, []() { \
            return std::make_shared<ClassName>(); \
        }); \
    return true; \
    }();