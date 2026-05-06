#pragma once
#include "../../config.h"
#include "../../component.h"
#include "../../physics/physicsSystem.h"
#include <Jolt/Physics/Character/CharacterVirtual.h>


class CharacterControllerComponent : public Component {
public:
    float moveSpeed     = 4.0f;
    float jumpImpulse   = 5.0f;
    float eyeHeight     = 0.8f;   // offset up from capsule center
    float capsuleRadius = 0.3f;
    float capsuleHalfH  = 0.45f;

    JPH::Ref<JPH::CharacterVirtual> character;

    void Init(PhysicsWorld& physics);

    void SetMoveInput(glm::vec3 dir, bool jump);

    void Start() override;

    void Update(float deltaTime) override;

    const char* GetTypeName() const override { return "CharacterController"; }

    ~CharacterControllerComponent() override;

private:
    glm::vec3 pendingMove { 0 };
    bool      pendingJump { false };
    PhysicsWorld* physicsWorld = nullptr;
    CharacterContactListenerImpl contactListener;
};

