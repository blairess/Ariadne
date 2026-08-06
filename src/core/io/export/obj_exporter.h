#pragma once

#include <string>
#include "../../terrain/terrain.h"

namespace Core::IO::Export {

    class OBJExporter {
    public:
        // Opens a native file dialog to pick destination and save the Terrain mesh
        static bool exportWithDialog(const Terrain& terrain);

        // Exports the given Terrain mesh directly to Wavefront .OBJ format at a specified path
        static bool exportToFile(const Terrain& terrain, const std::string& filePath);
    };

}