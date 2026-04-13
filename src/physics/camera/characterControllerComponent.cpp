#pragma once
#include "config.h"
#include "component.h"    // add this if it's not already there
#include "transform.h"
#include "physics/rigidbody.h"
#include "../physicsSystem.h"
#include "characterControllerComponent.h"
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>


void CharacterControllerComponent::Init(PhysicsWorld& physics) {
    physicsWorld = &physics;

    JPH::CapsuleShapeSettings capsuleSettings(capsuleHalfH, capsuleRadius);
    JPH::ShapeSettings::ShapeResult capsuleResult = capsuleSettings.Create();
    JPH::ShapeRefC capsuleShape = capsuleResult.Get();

    JPH::RotatedTranslatedShapeSettings rtsSettings(
        JPH::Vec3(0, capsuleHalfH + capsuleRadius, 0),
        JPH::Quat::sIdentity(),
        capsuleShape
    );
    JPH::ShapeRefC finalShape = rtsSettings.Create().Get();

    JPH::CharacterVirtualSettings settings;
    settings.mShape = finalShape;
    settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
    settings.mMaxStrength = 100.0f;
    settings.mCharacterPadding = 0.02f;
    settings.mPenetrationRecoverySpeed = 1.0f;
    settings.mUp = JPH::Vec3::sAxisY();
    settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -capsuleRadius);

    glm::vec3& pos = owner->transform.position;

    character = new JPH::CharacterVirtual(
        &settings,
        JPH::RVec3(pos.x, pos.y, pos.z),
        JPH::Quat::sIdentity(),
        0,
        physics.GetSystem()
        );

    character->SetListener(&contactListener);
}

void CharacterControllerComponent::SetMoveInput(glm::vec3 dir, bool jump) {
    pendingMove = dir;
    pendingJump = jump;
}

void CharacterControllerComponent::Start() {

}

void CharacterControllerComponent::Update(float deltaTime) {
    if (!character) return;

    // Build desired velocity
    JPH::Vec3 currentVel = character->GetLinearVelocity();

    glm::vec3 horizontal = pendingMove * moveSpeed;
    float verticalY = currentVel.GetY();

    if (character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround) {
        verticalY = 0.0f;
        if (pendingJump) {
            verticalY = jumpImpulse;
        }
    }

    JPH::Vec3 newVel(horizontal.x, verticalY, horizontal.z);
    newVel += character->GetUp() * (-9.81f * deltaTime);
    character->SetLinearVelocity(newVel);

    JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
        character->ExtendedUpdate(
        deltaTime,
        JPH::Vec3(0, -9.81f, 0),
        updateSettings,
        physicsWorld->GetSystem()->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
        physicsWorld->GetSystem()->GetDefaultLayerFilter(Layers::MOVING),
        {},
        {},
        *physicsWorld->GetTempAllocator()
        );
    // Sync our transform using JPH transform
    JPH::RVec3 p = character->GetPosition();
    owner->transform.position = glm::vec3(p.GetX(), p.GetY(), p.GetZ());


    // Reset inputs
    pendingMove = glm::vec3(0);
    pendingJump = false;
}

CharacterControllerComponent::~CharacterControllerComponent() {
}