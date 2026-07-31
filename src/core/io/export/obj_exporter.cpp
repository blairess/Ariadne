#include "obj_exporter.h"
#include <fstream>
#include <iostream>

namespace Core::IO::Export {

    bool OBJExporter::exportToFile(const Terrain& terrain, const std::string& filePath) {
        std::ofstream outFile(filePath);
        if (!outFile.is_open()) {
            std::cerr << "[OBJExporter] Failed to open file for writing: " << filePath << std::endl;
            return false;
        }

        // Header Comment
        outFile << "# Terrain Mesh Export\n";
        outFile << "# Units: Meters\n\n";

        const auto& vertices = terrain.vertices;

        // Write Geometric Vertices
        for (size_t i = 0; i < vertices.size(); i += 3) {
            outFile << "v " << vertices[i] << " " << vertices[i + 1] << " " << vertices[i + 2] << "\n";
        }

        outFile << "\n";

        // Write Faces
        int width = terrain.width;
        int depth = terrain.depth;

        for (int z = 0; z < depth - 1; ++z) {
            for (int x = 0; x < width - 1; ++x) {
                int topLeft = (z * width + x) + 1;
                int topRight = (z * width + (x + 1)) + 1;
                int bottomLeft = ((z + 1) * width + x) + 1;
                int bottomRight = ((z + 1) * width + (x + 1)) + 1;

                // First Triangle
                outFile << "f " << topLeft << " " << bottomLeft << " " << topRight << "\n";
                // Second Triangle
                outFile << "f " << topRight << " " << bottomLeft << " " << bottomRight << "\n";
            }
        }

        outFile.close();
        std::cout << "[OBJExporter] Successfully exported terrain to " << filePath << std::endl;
        return true;
    }

}