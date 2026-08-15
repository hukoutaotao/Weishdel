#pragma once

#include "core/config/ConfigError.hpp"
#include "core/model/Definitions.hpp"

#include <filesystem>
#include <vector>

namespace autochess::core
{
    // 此加载器按依赖顺序把技能、单位和分队配置转换为强类型定义。
    class DefinitionConfigLoader
    {
    public:
        // 此函数加载不依赖其他定义的技能配置。
        static bool loadSkills(
            const std::filesystem::path& sourcePath,
            std::vector<SkillDefinition>& skills,
            ConfigError& error);

        // 此函数加载单位配置并验证每个技能引用。
        static bool loadUnits(
            const std::filesystem::path& sourcePath,
            const std::vector<SkillDefinition>& skills,
            std::vector<UnitDefinition>& units,
            ConfigError& error);

        // 此函数加载分队和修正配置并验证全部跨定义引用。
        static bool loadFactions(
            const std::filesystem::path& sourcePath,
            const GameConfig& gameConfig,
            const std::vector<UnitDefinition>& units,
            std::vector<FactionDefinition>& factions,
            std::vector<FactionModifierDefinition>& modifiers,
            ConfigError& error);
    };
}
