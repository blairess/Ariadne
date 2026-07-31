#pragma once

#include <string>
#include "../../terrain/terrain.h"

namespace Core::IO::Export {

    class OBJExporter {
    public:
        // Exports the given Terrain mesh to Wavefront .OBJ format
        static bool exportToFile(const Terrain& terrain, const std::string& filePath);
    };

}