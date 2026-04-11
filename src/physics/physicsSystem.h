#pragma once
#include "config.h"
#include "scene.h"
#include "rigidbody.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

namespace Layers
{
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::uint NUM_LAYERS = 2;
};

// Determines which object layers can collide
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
    {
        switch (a)
        {
            case Layers::NON_MOVING: return b == Layers::MOVING;
            case Layers::MOVING:     return true;
            default:                 return false;
        }
    }
};

// Maps object layers to broadphase layers
namespace BPLayers
{
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr JPH::uint NUM_LAYERS = 2;
};

class BPLayerInterfaceImpl : public JPH::BroadPhaseLayerInterface
{
    JPH::BroadPhaseLayer layers[Layers::NUM_LAYERS];
public:
    BPLayerInterfaceImpl()
    {
        layers[Layers::NON_MOVING] = BPLayers::NON_MOVING;
        layers[Layers::MOVING]     = BPLayers::MOVING;
    }

    JPH::uint GetNumBroadPhaseLayers() const override { return BPLayers::NUM_LAYERS; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer l) const override { return layers[l]; }
};

class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer obj, JPH::BroadPhaseLayer bp) const override
    {
        switch (obj)
        {
            case Layers::NON_MOVING: return bp == BPLayers::MOVING;
            case Layers::MOVING:     return true;
            default:                 return false;
        }
    }
};

class PhysicsWorld
{
public:
    void Init()
    {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
        jobSystem = std::make_unique<JPH::JobSystemThreadPool>(
            JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
            std::thread::hardware_concurrency() - 1
        );

        const JPH::uint maxBodies = 4096;
        const JPH::uint numBodyMutexes = 0;   // auto
        const JPH::uint maxBodyPairs = 4096;
        const JPH::uint maxContactConstraints = 2048;

        physicsSystem.Init(maxBodies, numBodyMutexes, maxBodyPairs,
            maxContactConstraints, bpLayerInterface,
            objVsBpFilter, objLayerPairFilter);

        physicsSystem.SetGravity(JPH::Vec3(0, -9.81f, 0));
    }

    void AddBody(const std::shared_ptr<GameObject>& obj)
    {
        auto rb = obj->GetComponent<RigidBodyComponent>();
        if (!rb) return;

        JPH::ShapeRefC shape;
        switch (rb->body->shapeType)
        {
            case RigidBody::Box:
                shape = new JPH::BoxShape(JPH::Vec3(rb->body->halfExtent.x, rb->body->halfExtent.y, rb->body->halfExtent.z));
                break;
            case RigidBody::Sphere:
                shape = new JPH::SphereShape(rb->body->radius);
                break;
            case RigidBody::Capsule:
                shape = new JPH::CapsuleShape(rb->body->capsuleHalfHeight, rb->body->radius);
                break;
            default:
                shape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
                break;
        }

        JPH::Vec3 pos(obj->transform.position.x, obj->transform.position.y, obj->transform.position.z);
        JPH::Quat rot(obj->transform.rotation.x, obj->transform.rotation.y,
                      obj->transform.rotation.z, obj->transform.rotation.w);

        JPH::EMotionType motionType;
        JPH::ObjectLayer layer;
        switch (rb->body->motion)
        {
            case BodyMotion::Dynamic:
                motionType = JPH::EMotionType::Dynamic;
                layer = Layers::MOVING;
                break;
            case BodyMotion::Kinematic:
                motionType = JPH::EMotionType::Kinematic;
                layer = Layers::MOVING;
                break;
            default:
                motionType = JPH::EMotionType::Static;
                layer = Layers::NON_MOVING;
                break;
        }

        JPH::BodyCreationSettings settings(shape, pos, rot, motionType, layer);
        settings.mFriction = rb->body->friction;
        settings.mRestitution = rb->body->restitution;
        if (rb->body->motion == BodyMotion::Dynamic)
            settings.mMassPropertiesOverride.mMass = rb->body->mass;

        auto& bodyInterface = physicsSystem.GetBodyInterface();
        rb->body->bodyID = bodyInterface.CreateAndAddBody(settings, JPH::EActivation::Activate);
    }

    void RegisterScene(const std::shared_ptr<Scene>& scene)
    {
        for (const auto& obj : scene->objects) {
            auto rb = obj->GetComponent<RigidBodyComponent>();
                if (rb) AddBody(obj);
        }


        physicsSystem.OptimizeBroadPhase();
    }

    void Update(float deltaTime)
    {
        accumulator += deltaTime;
        while (accumulator >= fixedStep)
        {
            physicsSystem.Update(fixedStep, 1, tempAllocator.get(), jobSystem.get());
            accumulator -= fixedStep;
        }
    }

    void SyncTransforms(const std::shared_ptr<Scene>& scene)
    {
        auto& bodyInterface = physicsSystem.GetBodyInterface();

        for (const auto& obj : scene->objects)
        {
            auto rb = obj->GetComponent<RigidBodyComponent>();
            if (!rb || rb->body->motion == BodyMotion::Static)
                continue;

            auto id = rb->body->bodyID;

            // Jolt -> glm
            JPH::Vec3 pos = bodyInterface.GetPosition(id);
            JPH::Quat rot = bodyInterface.GetRotation(id);

            obj->transform.position = glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
            obj->transform.rotation = glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
        }
    }

    void RemoveBody(const std::shared_ptr<RigidBody>& rb)
    {
        auto& bodyInterface = physicsSystem.GetBodyInterface();
        bodyInterface.RemoveBody(rb->bodyID);
        bodyInterface.DestroyBody(rb->bodyID);
    }

    void Shutdown()
    {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    JPH::PhysicsSystem& GetSystem() { return physicsSystem; }


private:
    JPH::PhysicsSystem physicsSystem;
    BPLayerInterfaceImpl bpLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl objVsBpFilter;
    ObjectLayerPairFilterImpl objLayerPairFilter;

    std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> jobSystem;

    float fixedStep = 1.0f / 60.0f;
    float accumulator = 0.0f;
};