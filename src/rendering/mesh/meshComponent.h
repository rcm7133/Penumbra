#pragma once
#include "../../component.h"
#include "model.h"

class MeshComponent : public Component
{
public:
    std::shared_ptr<Model> model;

    MeshComponent(std::shared_ptr<Model> m) : model(std::move(m)) {}

    void Update(float dt) override {}
    void Start() override {}

    const char* GetTypeName() const override { return "MeshComponent"; }
};