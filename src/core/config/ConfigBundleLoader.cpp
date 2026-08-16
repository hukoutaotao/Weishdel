#include "core/config/ConfigBundleLoader.hpp"

#include "core/config/DefinitionConfigLoader.hpp"
#include "core/config/GameConfigLoader.hpp"
#include "core/config/MapConfigLoader.hpp"

#include <utility>

namespace autochess::core
{
    // 此函数按配置依赖顺序加载整包数据，并保证任一步失败都不覆盖原结果。
    bool ConfigBundleLoader::load(
        const std::filesystem::path& dataDirectory,
        ConfigBundle& bundle,
        ConfigError& error)
    {
        // 此局部对象暂存本次结果，从而提供全有或全无的加载语义。
        ConfigBundle parsedBundle;

        // 此代码段依次加载游戏、技能、单位和分队配置并验证跨文件引用。
        if (!GameConfigLoader::load(
                dataDirectory / "game.cfg",
                parsedBundle.gameConfig,
                error)
            || !DefinitionConfigLoader::loadSkills(
                dataDirectory / "skills.cfg",
                parsedBundle.skills,
                error)
            || !DefinitionConfigLoader::loadUnits(
                dataDirectory / "units.cfg",
                parsedBundle.skills,
                parsedBundle.units,
                error)
            || !DefinitionConfigLoader::loadFactions(
                dataDirectory / "factions.cfg",
                parsedBundle.gameConfig,
                parsedBundle.units,
                parsedBundle.factions,
                parsedBundle.factionModifiers,
                error))
        {
            return false;
        }

        // 此代码段建立两张正式地图的固定路径以保证加载顺序可重复。
        const std::filesystem::path firstMapPath =
            dataDirectory / "maps" / "map_01.map";
        const std::filesystem::path secondMapPath =
            dataDirectory / "maps" / "map_02.map";

        // 此代码段加载第一张对称双路线地图并立即传播详细错误。
        MapDefinition firstMap;
        if (!MapConfigLoader::load(firstMapPath, firstMap, error))
        {
            return false;
        }

        // 此代码段加载第二张路线长度差异地图并立即传播详细错误。
        MapDefinition secondMap;
        if (!MapConfigLoader::load(secondMapPath, secondMap, error))
        {
            return false;
        }

        // 此代码段拒绝两张独立地图文件声明相同的全局地图 ID。
        if (firstMap.id == secondMap.id)
        {
            error.category = ConfigErrorCategory::DuplicateDefinition;
            error.sourcePath = secondMapPath;
            error.line = 0;
            error.message = "地图 ID " + secondMap.id + " 在多张地图中重复";
            return false;
        }

        // 此代码段按 map_01、map_02 的固定顺序保存完整地图集合。
        parsedBundle.maps.push_back(std::move(firstMap));
        parsedBundle.maps.push_back(std::move(secondMap));

        // 此赋值只在全部加载成功后一次性提交结果以保持原子性。
        bundle = std::move(parsedBundle);
        return true;
    }
}
