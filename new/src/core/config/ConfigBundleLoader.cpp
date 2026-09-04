#include "core/config/ConfigBundleLoader.hpp"

#include "core/config/DefinitionConfigLoader.hpp"
#include "core/config/GameConfigLoader.hpp"
#include "core/config/MapConfigLoader.hpp"

#include <array>
#include <set>
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

        // 此代码段依次加载游戏、单位和分队配置并验证跨文件引用。
        if (!GameConfigLoader::load(
                dataDirectory / "game.cfg",
                parsedBundle.gameConfig,
                error)
            || !DefinitionConfigLoader::loadUnits(
                dataDirectory / "units.cfg",
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

        // 此代码段建立四张正式地图的固定路径以保证加载顺序可重复。
        const std::array<std::filesystem::path, 4> mapPaths = {
            dataDirectory / "maps" / "map_01.map",
            dataDirectory / "maps" / "map_02.map",
            dataDirectory / "maps" / "map_03.map",
            dataDirectory / "maps" / "map_04.map"};

        // 此循环逐张加载地图，并在加入整包前拒绝全局重复地图 ID。
        std::set<std::string> loadedMapIds;
        for (const std::filesystem::path& mapPath : mapPaths)
        {
            MapDefinition map;
            if (!MapConfigLoader::load(mapPath, map, error))
            {
                return false;
            }

            if (!loadedMapIds.insert(map.id).second)
            {
                error.category = ConfigErrorCategory::DuplicateDefinition;
                error.sourcePath = mapPath;
                error.line = 0;
                error.message = "地图 ID " + map.id + " 在多张地图中重复";
                return false;
            }

            parsedBundle.maps.push_back(std::move(map));
        }

        // 此赋值只在全部加载成功后一次性提交结果以保持原子性。
        bundle = std::move(parsedBundle);
        return true;
    }
}
