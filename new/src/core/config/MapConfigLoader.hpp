#pragma once

#include "core/config/ConfigError.hpp"
#include "core/map/MapTypes.hpp"

#include <filesystem>

namespace autochess::core
{
    class MapConfigLoader
    {
    public:
        static bool load(
            const std::filesystem::path& sourcePath,
            MapDefinition& map,
            ConfigError& error);
    };
}
