#include "flatten_brush.h"
#include <cmath>
#include <algorithm>

namespace Core::Tools {

    FlattenBrush::FlattenBrush() {
        radius = 5.0f;
        strength = 1.0f;
    }

    void FlattenBrush::sampleTargetHeight(const Terrain& terrain, const glm::vec3& hitPoint) {
        float max_x = (terrain.width - 1) * terrain.cellSize;
        float max_z = (terrain.depth - 1) * terrain.cellSize;

        if (hitPoint.x < 0.0f || hitPoint.x > max_x || hitPoint.z < 0.0f || hitPoint.z > max_z) {
            return;
        }

        int gx = static_cast<int>(hitPoint.x / terrain.cellSize);
        int gz = static_cast<int>(hitPoint.z / terrain.cellSize);

        gx = std::clamp(gx, 0, terrain.width - 1);
        gz = std::clamp(gz, 0, terrain.depth - 1);

        int index = (gz * terrain.width + gx) * 3 + 1; // Y height coordinate
        targetHeight = terrain.vertices[index];
        targetSampled = true;
    }

    void FlattenBrush::apply(Terrain& terrain, const glm::vec3& hitPoint, float deltaTime) {
        // Sample height once at the start of the drag gesture
        if (!targetSampled) {
            sampleTargetHeight(terrain, hitPoint);
        }

        // Bounding box optimization around brush radius
        int minX = static_cast<int>(std::floor((hitPoint.x - radius) / terrain.cellSize));
        int maxX = static_cast<int>(std::ceil((hitPoint.x + radius) / terrain.cellSize));
        int minZ = static_cast<int>(std::floor((hitPoint.z - radius) / terrain.cellSize));
        int maxZ = static_cast<int>(std::ceil((hitPoint.z + radius) / terrain.cellSize));

        minX = std::clamp(minX, 0, terrain.width - 1);
        maxX = std::clamp(maxX, 0, terrain.width - 1);
        minZ = std::clamp(minZ, 0, terrain.depth - 1);
        maxZ = std::clamp(maxZ, 0, terrain.depth - 1);

        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                int vertexIndex = (z * terrain.width + x) * 3;

                float vx = terrain.vertices[vertexIndex];
                float vz = terrain.vertices[vertexIndex + 2];

                float dx = vx - hitPoint.x;
                float dz = vz - hitPoint.z;
                float dist = std::sqrt(dx * dx + dz * dz);

                if (dist <= radius) {
                    float normalizedDist = dist / radius;
                    // Cosine falloff formula for smooth blending
                    float falloff = 0.5f * (1.0f + std::cos(3.14159265f * normalizedDist));

                    float currentHeight = terrain.vertices[vertexIndex + 1];
                    float blendFactor = std::clamp(strength * falloff * deltaTime * 5.0f, 0.0f, 1.0f);

                    // Move current height towards target height
                    terrain.vertices[vertexIndex + 1] = currentHeight + (targetHeight - currentHeight) * blendFactor;
                }
            }
        }

        terrain.setupBuffers();
    }

}