#pragma once
#include "../component.h"
#include "mesh.h"

class MeshComponent : public Component
{
public:
    std::shared_ptr<Mesh> mesh;

    MeshComponent(const std::string& model, const std::string& texture,
                  float shininess = 32.0f, const std::string& normalMap = "")
        : mesh(std::make_shared<Mesh>(model, texture, shininess, normalMap)) {}

    MeshComponent(std::shared_ptr<Mesh> existing)
        : mesh(std::move(existing)) {}

    const char* GetTypeName() const override { return "Mesh"; }
};