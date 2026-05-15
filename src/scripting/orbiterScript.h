#pragma once
#include "core/scriptComponent.h"
#include "core/scriptRegistry.h"

class OrbiterScript : public ScriptComponent
{
public:
    glm::vec3 center      = glm::vec3(0.0f);
    float     radius      = 3.0f;
    float     speed       = 1.0f;   // rads / second
    float     heightOffset = 0.0f;  // Y offset from center

private:
    float angle = 0.0f;

public:
    OrbiterScript() { scriptName = "OrbiterScript"; }

    void Start() override {
        glm::vec3 offset = transform().position - center;
        angle = std::atan2(offset.z, offset.x);
    }

    void Update(float dt) override {
        angle += speed * dt;

        transform().position = glm::vec3(
            center.x + std::cos(angle) * radius,
            center.y + heightOffset,
            center.z + std::sin(angle) * radius
        );
    }

    nlohmann::json Serialize() const override {
        return {
            {"center", {center.x,center.y,center.z}},
            {"radius", radius},
            {"speed",speed},
            {"heightOffset",heightOffset}
        };
    }

    void Deserialize(const nlohmann::json& j) override {
        if (j.contains("center")) {
            center = { j["center"][0], j["center"][1], j["center"][2] };
        }
        radius = j.value("radius",3.0f);
        speed = j.value("speed",1.0f);
        heightOffset = j.value("heightOffset",0.0f);
    }

    const char* GetScriptName() const override { return "OrbiterScript"; }
};

REGISTER_SCRIPT(OrbiterScript)