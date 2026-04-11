#pragma once
#include "../component.h"
#include "rigidbody.h"

class RigidBodyComponent : public Component
{
public:
    std::shared_ptr<RigidBody> body;

    RigidBodyComponent() : body(std::make_shared<RigidBody>()) {}

    const char* GetTypeName() const override { return "RigidBody"; }
};