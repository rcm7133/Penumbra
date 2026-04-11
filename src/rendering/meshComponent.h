#pragma once
#include "../component.h"
#include "mesh.h"

class MeshComponent : public Component
{
public:
    std::shared_ptr<Mesh> mesh;

    MeshComponent(const std::string& model, const std::string& texture, const std::string& normalMap = "")
        : mesh(std::make_shared<Mesh>(model, texture, normalMap)) {}

    MeshComponent(std::shared_ptr<Mesh> existing)
        : mesh(std::move(existing)) {}

    const char* GetTypeName() const override { return "Mesh"; }
};