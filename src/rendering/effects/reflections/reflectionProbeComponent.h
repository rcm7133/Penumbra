#pragma once
#include "reflectionProbe.h"
#include "../../../config.h"
#include "../../../component.h"

class ReflectionProbeComponent : public Component {
public:
    std::shared_ptr<ReflectionProbe> probe;

    ReflectionProbeComponent() : probe(std::make_shared<ReflectionProbe>()) {}

    void Update(float dt) override {}
    void Start() override {}

    const char* GetTypeName() const override { return "ReflectionProbeComponent"; }
};
