#pragma once
#include "config.h"
#include "rendering/mesh.h"
#include "transform.h"
#include "rendering/light.h"
#include "particles/particleSystem.h"
#include "physics/rigidbody.h"
#include "rendering/fogVolume.h"

class GameObject
{
public:
    Transform transform;
    std::shared_ptr<Mesh> mesh = nullptr;
    std::shared_ptr<Light> light   = nullptr;
    std::shared_ptr<ParticleSystem> particleSystem = nullptr;
    std::shared_ptr<RigidBody> rigidBody = nullptr;
    std::shared_ptr<FogVolume> fogVolume = nullptr;
    bool enabled = true;

    std::string name = "GameObject";
    GameObject(const std::string& name = "GameObject") : name(name) {}
    virtual ~GameObject() {}

    virtual void Update(float deltaTime) {}
    virtual void Start() {}
};

class Orbiter : public GameObject
{
public:
    glm::vec3 orbitCenter = glm::vec3(0, 0, 0);
    float orbitRadius     = 3.0f;
    float orbitSpeed      = 45.0f; // degrees per second
    float orbitAngle      = 0.0f;

    Orbiter(const std::string& name = "Orbiter") : GameObject(name) {}

    void Update(float deltaTime) override
    {
        orbitAngle += orbitSpeed * deltaTime;
        if (orbitAngle > 360.0f) orbitAngle -= 360.0f;

        float rad = glm::radians(orbitAngle);
        transform.position.x = orbitCenter.x + orbitRadius * cos(rad);
        transform.position.z = orbitCenter.z + orbitRadius * sin(rad);
        transform.position.y = orbitCenter.y; // stays at same height
    }

    void Start() override {

    }
};

class RotatingLight : public GameObject
{
public:
    float rotationSpeed = 90.0f;      // degrees per second
    float rotationAngle = 0.0f;       // current rotation angle

    RotatingLight(const std::string& name = "RotatingLight") : GameObject(name) {}

    void Update(float deltaTime) override
    {
        rotationAngle += rotationSpeed * deltaTime;
        if (rotationAngle >= 360.0f)
            rotationAngle -= 360.0f;

        transform.rotation = glm::angleAxis(glm::radians(rotationAngle), glm::vec3(0, 1, 0));

        if (light)
            light->direction = transform.Forward();
    }

    void Start() override {

    }
};