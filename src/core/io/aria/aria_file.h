#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>

namespace Core::IO::Aria {

    constexpr const char* FILE_EXTENSION = ".aria";            // Default file extension
    constexpr char MAGIC_HEADER[4] = { 'A', 'R', 'I', 'A' };   // Magic bytes identifying the format
    constexpr uint32_t FORMAT_VERSION = 1;                     // Current serializer version


    // TODO: I have no idea whats next...
}