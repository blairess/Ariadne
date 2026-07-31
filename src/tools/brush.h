#pragma once

#include <glm/glm.hpp>

class Terrain;

namespace Core::Tools {

    class Brush {
    public:
        Brush() = default;
        virtual ~Brush() = default;

        // Interface method to apply the brush effect to the terrain
        virtual void apply(Terrain& terrain, const glm::vec3& hitPosition, float deltaTime) = 0;

        // Brush parameters
        float radius = 0.3f;
        float strength = 1.0f;
    };

}