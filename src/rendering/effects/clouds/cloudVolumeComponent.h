#pragma once
#include "cloudVolume.h"
#include "../../../component.h"
#include "cloudViewer.h"


class CloudVolumeComponent : public Component {
public:
    std::shared_ptr<CloudVolume> volume;
    CloudTextureViewer viewer;

    CloudVolumeComponent() : volume(std::make_shared<CloudVolume>()) {}

    const char* GetTypeName() const override { return "CloudVolume"; }
};
