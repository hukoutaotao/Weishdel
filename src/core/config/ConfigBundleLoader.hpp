#pragma once

#include "core/config/ConfigError.hpp"
#include "core/map/MapTypes.hpp"
#include "core/model/Definitions.hpp"

#include <filesystem>
#include <vector>

namespace autochess::core
{
    // 此结构集中保存一次游戏启动所需的全部已校验配置数据。
    struct ConfigBundle
    {
        // 此字段保存对局、经济和容量等全局游戏参数。
        GameConfig gameConfig;

        // 此代码段保存按依赖顺序加载的技能、单位和分队定义。
        std::vector<SkillDefinition> skills;
        std::vector<UnitDefinition> units;
        std::vector<FactionDefinition> factions;
        std::vector<FactionModifierDefinition> factionModifiers;

        // 此字段保存顺序固定的四张正式地图定义。
        std::vector<MapDefinition> maps;
    };

    // 此加载器负责按冻结顺序原子地加载全部正式配置文件。
    class ConfigBundleLoader
    {
    public:
        // 此函数仅在全部文件加载和跨文件校验成功后覆盖调用方结果。
        static bool load(
            const std::filesystem::path& dataDirectory,
            ConfigBundle& bundle,
            ConfigError& error);
    };
}
