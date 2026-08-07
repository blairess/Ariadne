#pragma once

#include "../brush.h"
#include "../../core/terrain/terrain.h"
#include <glm/glm.hpp>

namespace Core::Tools {

    class FlattenBrush : public Brush {
    public:
        float targetHeight = 0.0f;
        bool targetSampled = false;

        FlattenBrush();

        void apply(Terrain& terrain, const glm::vec3& hitPoint, float deltaTime) override;
        void sampleTargetHeight(const Terrain& terrain, const glm::vec3& hitPoint);
        void resetTarget() { targetSampled = false; }
    };

}