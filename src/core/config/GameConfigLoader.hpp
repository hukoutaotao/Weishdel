#pragma once

#include "core/config/ConfigError.hpp"
#include "core/model/Definitions.hpp"

#include <filesystem>

namespace autochess::core
{
    // 此加载器负责把 game.cfg 严格转换为通过语义校验的 GameConfig。
    class GameConfigLoader
    {
    public:
        static bool load(
            const std::filesystem::path& sourcePath,
            GameConfig& config,
            ConfigError& error);
    };
}
