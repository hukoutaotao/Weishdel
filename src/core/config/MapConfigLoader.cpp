#include "core/config/MapConfigLoader.hpp"

#include "core/config/ConfigParser.hpp"
#include "core/config/ConfigValueReader.hpp"

#include <charconv>
#include <set>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace autochess::core
{
    namespace
    {
        enum class CoordinateParseResult
        {
            Success,
            InvalidFormat,
            OutOfRange
        };

        const std::set<std::string>& routeAllowedFields()
        {
            static const std::set<std::string> fields = {
                "side",
                "start",
                "points"};
            return fields;
        }

        CoordinateParseResult parseCoordinateComponent(
            const std::string& text,
            int& value)
        {
            if (text.empty() || text.front() == '+')
            {
                return CoordinateParseResult::InvalidFormat;
            }

            int parsedValue = 0;
            const char* const first = text.data();
            const char* const last = first + text.size();
            const auto result = std::from_chars(first, last, parsedValue);
            if (result.ec == std::errc::result_out_of_range)
            {
                return CoordinateParseResult::OutOfRange;
            }
            if (result.ec != std::errc{} || result.ptr != last)
            {
                return CoordinateParseResult::InvalidFormat;
            }

            value = parsedValue;
            return CoordinateParseResult::Success;
        }

        CoordinateParseResult parseGridPositionText(
            const std::string& text,
            GridPosition& position)
        {
            const std::size_t separator = text.find(',');
            if (separator == std::string::npos
                || text.find(',', separator + 1) != std::string::npos)
            {
                return CoordinateParseResult::InvalidFormat;
            }

            GridPosition parsedPosition;
            const CoordinateParseResult xResult = parseCoordinateComponent(
                text.substr(0, separator), parsedPosition.x);
            if (xResult != CoordinateParseResult::Success)
            {
                return xResult;
            }

            const CoordinateParseResult yResult = parseCoordinateComponent(
                text.substr(separator + 1), parsedPosition.y);
            if (yResult != CoordinateParseResult::Success)
            {
                return yResult;
            }

            position = parsedPosition;
            return CoordinateParseResult::Success;
        }

        bool readGridPositionField(
            const ConfigSection& section,
            const std::filesystem::path& path,
            const std::string& key,
            GridPosition& position,
            ConfigError& error)
        {
            const ConfigField* const field = findRequiredConfigField(
                section, key, path, error);
            if (field == nullptr)
            {
                return false;
            }

            GridPosition parsedPosition;
            const CoordinateParseResult result = parseGridPositionText(
                field->value, parsedPosition);
            if (result != CoordinateParseResult::Success)
            {
                return failConfig(
                    error,
                    result == CoordinateParseResult::OutOfRange
                        ? ConfigErrorCategory::RangeError
                        : ConfigErrorCategory::TypeError,
                    path,
                    field->line,
                    result == CoordinateParseResult::OutOfRange
                        ? "字段 " + key + " 的坐标超出整数范围"
                        : "字段 " + key + " 必须是 x,y 格式的整数坐标");
            }

            position = parsedPosition;
            return true;
        }

        bool readGridPositionListField(
            const ConfigSection& section,
            const std::filesystem::path& path,
            const std::string& key,
            std::vector<GridPosition>& positions,
            ConfigError& error)
        {
            const ConfigField* const field = findRequiredConfigField(
                section, key, path, error);
            if (field == nullptr)
            {
                return false;
            }

            std::vector<GridPosition> parsedPositions;
            std::size_t start = 0;
            while (start <= field->value.size())
            {
                const std::size_t separator = field->value.find(';', start);
                const std::size_t end = separator == std::string::npos
                    ? field->value.size()
                    : separator;
                const std::string coordinate = field->value.substr(
                    start, end - start);

                GridPosition position;
                const CoordinateParseResult result = parseGridPositionText(
                    coordinate, position);
                if (result != CoordinateParseResult::Success)
                {
                    return failConfig(
                        error,
                        result == CoordinateParseResult::OutOfRange
                            ? ConfigErrorCategory::RangeError
                            : ConfigErrorCategory::TypeError,
                        path,
                        field->line,
                        result == CoordinateParseResult::OutOfRange
                            ? "字段 " + key + " 的坐标列表包含超出整数范围的值"
                            : "字段 " + key + " 必须是以分号分隔的 x,y 坐标列表");
                }

                parsedPositions.push_back(position);
                if (separator == std::string::npos)
                {
                    break;
                }
                start = separator + 1;
            }

            if (parsedPositions.size() < 2)
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::MapValidation,
                    path,
                    field->line,
                    "字段 points 至少需要包含起点和终点");
            }

            positions = std::move(parsedPositions);
            return true;
        }

        bool parseMapSide(const std::string& text, MapSide& side)
        {
            if (text == "A")
            {
                side = MapSide::A;
                return true;
            }
            if (text == "B")
            {
                side = MapSide::B;
                return true;
            }
            return false;
        }

        bool rejectUnknownMapFields(
            const ConfigSection& section,
            const std::filesystem::path& path,
            const int height,
            ConfigError& error)
        {
            static const std::set<std::string> fixedFields = {
                "id",
                "name",
                "width",
                "height"};

            for (const auto& field : section.fields)
            {
                if (fixedFields.find(field.first) != fixedFields.end())
                {
                    continue;
                }

                if (field.first.rfind("row_", 0) != 0)
                {
                    return failConfig(
                        error,
                        ConfigErrorCategory::UnknownField,
                        path,
                        field.second.line,
                        "未知的地图配置字段 " + field.first);
                }

                int rowIndex = 0;
                const CoordinateParseResult result = parseCoordinateComponent(
                    field.first.substr(4), rowIndex);
                if (result != CoordinateParseResult::Success
                    || rowIndex < 0
                    || rowIndex >= height
                    || field.first != "row_" + std::to_string(rowIndex))
                {
                    return failConfig(
                        error,
                        ConfigErrorCategory::UnknownField,
                        path,
                        field.second.line,
                        "地图行字段必须从 row_0 连续写到 row_"
                            + std::to_string(height - 1));
                }
            }

            return true;
        }

        bool readMapSection(
            const ConfigSection& section,
            const std::filesystem::path& path,
            MapDefinition& map,
            ConfigError& error)
        {
            MapDefinition parsedMap;
            if (!readStringConfigField(section, path, "id", parsedMap.id, error)
                || !readStringConfigField(section, path, "name", parsedMap.name, error)
                || !readIntConfigField(section, path, "width", parsedMap.width, error)
                || !readIntConfigField(section, path, "height", parsedMap.height, error))
            {
                return false;
            }

            if (!isConfigIdentifier(parsedMap.id))
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::TypeError,
                    path,
                    section.fields.at("id").line,
                    "字段 id 必须符合小写 snake_case 规则");
            }

            if (!requireConfigRange(
                    parsedMap.width >= 3,
                    section,
                    path,
                    "width",
                    "必须大于或等于 3",
                    error)
                || !requireConfigRange(
                    parsedMap.height >= 3,
                    section,
                    path,
                    "height",
                    "必须大于或等于 3",
                    error)
                || !rejectUnknownMapFields(
                    section, path, parsedMap.height, error))
            {
                return false;
            }

            int deploymentACount = 0;
            int deploymentBCount = 0;
            int guardXCount = 0;
            int guardYCount = 0;

            for (int rowIndex = 0; rowIndex < parsedMap.height; ++rowIndex)
            {
                const std::string key = "row_" + std::to_string(rowIndex);
                const ConfigField* const field = findRequiredConfigField(
                    section, key, path, error);
                if (field == nullptr)
                {
                    return false;
                }

                if (field->value.size()
                    != static_cast<std::size_t>(parsedMap.width))
                {
                    return failConfig(
                        error,
                        ConfigErrorCategory::MapValidation,
                        path,
                        field->line,
                        "地图行 " + key + " 的字符数必须等于 width");
                }

                for (const char cell : field->value)
                {
                    if (std::string(".#ABXY").find(cell) == std::string::npos)
                    {
                        return failConfig(
                            error,
                            ConfigErrorCategory::MapValidation,
                            path,
                            field->line,
                            "地图行 " + key + " 包含未知网格字符");
                    }

                    deploymentACount += cell == 'A' ? 1 : 0;
                    deploymentBCount += cell == 'B' ? 1 : 0;
                    guardXCount += cell == 'X' ? 1 : 0;
                    guardYCount += cell == 'Y' ? 1 : 0;
                }

                parsedMap.gridRows.push_back(field->value);
            }

            if (deploymentACount == 0 || deploymentBCount == 0)
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::MapValidation,
                    path,
                    section.line,
                    "地图必须至少包含一个 A 方部署格和一个 B 方部署格");
            }
            if (guardXCount != 1 || guardYCount != 1)
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::MapValidation,
                    path,
                    section.line,
                    "地图必须恰好包含一个 X 守卫格和一个 Y 守卫格");
            }

            map = std::move(parsedMap);
            return true;
        }

        bool readRouteSection(
            const ConfigSection& section,
            const std::filesystem::path& path,
            Route& route,
            ConfigError& error)
        {
            if (!rejectUnknownConfigFields(
                    section,
                    path,
                    routeAllowedFields(),
                    "路线",
                    error))
            {
                return false;
            }

            std::string sideText;
            Route parsedRoute;
            parsedRoute.id = section.id;
            if (!readStringConfigField(section, path, "side", sideText, error)
                || !readGridPositionField(
                    section, path, "start", parsedRoute.start, error)
                || !readGridPositionListField(
                    section, path, "points", parsedRoute.points, error))
            {
                return false;
            }

            if (!parseMapSide(sideText, parsedRoute.side))
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::TypeError,
                    path,
                    section.fields.at("side").line,
                    "字段 side 只能是 A 或 B");
            }

            route = std::move(parsedRoute);
            return true;
        }

        // 此函数判断一个格子坐标是否位于地图声明的宽高范围内。
        bool isInsideMap(
            const MapDefinition& map,
            const GridPosition& position) noexcept
        {
            return position.x >= 0
                && position.y >= 0
                && position.x < map.width
                && position.y < map.height;
        }

        // 此函数在调用方确认坐标合法后读取对应的地图网格字符。
        char mapCellAt(
            const MapDefinition& map,
            const GridPosition& position)
        {
            return map.gridRows[static_cast<std::size_t>(position.y)]
                [static_cast<std::size_t>(position.x)];
        }

        // 此函数把格子坐标转换为适合写入中文错误消息的文本。
        std::string formatGridPosition(const GridPosition& position)
        {
            return std::to_string(position.x)
                + ","
                + std::to_string(position.y);
        }

        // 此函数判断路线阵营与起点部署格字符是否一致。
        bool isOwnDeploymentCell(const MapSide side, const char cell) noexcept
        {
            return (side == MapSide::A && cell == 'A')
                || (side == MapSide::B && cell == 'B');
        }

        // 此函数返回指定阵营路线必须抵达的敌方守卫格字符。
        char enemyGuardCell(const MapSide side) noexcept
        {
            return side == MapSide::A ? 'Y' : 'X';
        }

        // 此函数按冻结顺序验证路线起点、部署覆盖、边界、障碍、相邻关系和终点。
        bool validateMapRoutes(
            const MapDefinition& map,
            const ConfigSection& mapSection,
            const std::vector<const ConfigSection*>& routeSections,
            const std::filesystem::path& path,
            ConfigError& error)
        {
            // 此代码段先确认每条路线坐标列表的首点与声明起点完全一致。
            for (std::size_t index = 0; index < map.routes.size(); ++index)
            {
                const Route& route = map.routes[index];
                const ConfigSection& section = *routeSections[index];
                if (route.points.front() != route.start)
                {
                    return failConfig(
                        error,
                        ConfigErrorCategory::MapValidation,
                        path,
                        section.fields.at("points").line,
                        "路线 " + route.id
                            + " 的 points 首点必须等于 start");
                }
            }

            // 此代码段确认每条路线的起点在地图内并位于本阵营部署格。
            for (std::size_t index = 0; index < map.routes.size(); ++index)
            {
                const Route& route = map.routes[index];
                const ConfigSection& section = *routeSections[index];
                if (!isInsideMap(map, route.start))
                {
                    return failConfig(
                        error,
                        ConfigErrorCategory::MapValidation,
                        path,
                        section.fields.at("start").line,
                        "路线 " + route.id + " 的起点 "
                            + formatGridPosition(route.start) + " 越界");
                }

                if (!isOwnDeploymentCell(
                        route.side, mapCellAt(map, route.start)))
                {
                    return failConfig(
                        error,
                        ConfigErrorCategory::MapValidation,
                        path,
                        section.fields.at("start").line,
                        "路线 " + route.id
                            + " 的起点必须是本阵营部署格");
                }
            }

            // 此代码段逐格统计对应路线，保证每个合法部署格恰好拥有一条路线。
            for (int y = 0; y < map.height; ++y)
            {
                for (int x = 0; x < map.width; ++x)
                {
                    const char cell = map.gridRows[static_cast<std::size_t>(y)]
                        [static_cast<std::size_t>(x)];
                    if (cell != 'A' && cell != 'B')
                    {
                        continue;
                    }

                    const MapSide side = cell == 'A' ? MapSide::A : MapSide::B;
                    const GridPosition deployment{x, y};
                    int matchingRouteCount = 0;

                    // 此代码段统计与当前部署格的阵营和起点同时匹配的路线数量。
                    for (const Route& route : map.routes)
                    {
                        if (route.side == side && route.start == deployment)
                        {
                            ++matchingRouteCount;
                        }
                    }

                    if (matchingRouteCount != 1)
                    {
                        const std::string rowKey = "row_" + std::to_string(y);
                        return failConfig(
                            error,
                            ConfigErrorCategory::MapValidation,
                            path,
                            mapSection.fields.at(rowKey).line,
                            "部署格 " + formatGridPosition(deployment)
                                + (matchingRouteCount == 0
                                    ? " 没有对应路线"
                                    : " 对应多条路线"));
                    }
                }
            }

            // 此代码段检查所有路线点均位于地图边界内，避免后续读取网格时越界。
            for (std::size_t index = 0; index < map.routes.size(); ++index)
            {
                const Route& route = map.routes[index];
                const ConfigSection& section = *routeSections[index];
                for (const GridPosition& point : route.points)
                {
                    if (!isInsideMap(map, point))
                    {
                        return failConfig(
                            error,
                            ConfigErrorCategory::MapValidation,
                            path,
                            section.fields.at("points").line,
                            "路线 " + route.id + " 的坐标 "
                                + formatGridPosition(point) + " 越界");
                    }
                }
            }

            // 此代码段拒绝任何穿过障碍字符井号的路线。
            for (std::size_t index = 0; index < map.routes.size(); ++index)
            {
                const Route& route = map.routes[index];
                const ConfigSection& section = *routeSections[index];
                for (const GridPosition& point : route.points)
                {
                    if (mapCellAt(map, point) == '#')
                    {
                        return failConfig(
                            error,
                            ConfigErrorCategory::MapValidation,
                            path,
                            section.fields.at("points").line,
                            "路线 " + route.id + " 穿过障碍格 "
                                + formatGridPosition(point));
                    }
                }
            }

            // 此代码段使用曼哈顿距离检查路线只能进行上下左右相邻移动。
            for (std::size_t index = 0; index < map.routes.size(); ++index)
            {
                const Route& route = map.routes[index];
                const ConfigSection& section = *routeSections[index];
                for (std::size_t pointIndex = 1;
                    pointIndex < route.points.size();
                    ++pointIndex)
                {
                    const GridPosition& previous = route.points[pointIndex - 1];
                    const GridPosition& current = route.points[pointIndex];
                    const long long xDistance = static_cast<long long>(current.x)
                        - static_cast<long long>(previous.x);
                    const long long yDistance = static_cast<long long>(current.y)
                        - static_cast<long long>(previous.y);
                    const long long manhattanDistance =
                        (xDistance < 0 ? -xDistance : xDistance)
                        + (yDistance < 0 ? -yDistance : yDistance);
                    if (manhattanDistance != 1)
                    {
                        return failConfig(
                            error,
                            ConfigErrorCategory::MapValidation,
                            path,
                            section.fields.at("points").line,
                            "路线 " + route.id + " 包含非法连接："
                                + formatGridPosition(previous) + " -> "
                                + formatGridPosition(current));
                    }
                }
            }

            // 此代码段确认每条路线最终抵达对应的敌方守卫格。
            for (std::size_t index = 0; index < map.routes.size(); ++index)
            {
                const Route& route = map.routes[index];
                const ConfigSection& section = *routeSections[index];
                if (mapCellAt(map, route.points.back())
                    != enemyGuardCell(route.side))
                {
                    return failConfig(
                        error,
                        ConfigErrorCategory::MapValidation,
                        path,
                        section.fields.at("points").line,
                        "路线 " + route.id + " 的终点必须是敌方守卫格");
                }
            }

            return true;
        }
    }

    bool MapConfigLoader::load(
        const std::filesystem::path& sourcePath,
        MapDefinition& map,
        ConfigError& error)
    {
        ConfigDocument document;
        if (!ConfigParser::parse(sourcePath, document, error))
        {
            return false;
        }

        const ConfigSection* mapSection = nullptr;
        std::vector<const ConfigSection*> routeSections;
        for (const ConfigSection& section : document.sections)
        {
            if (section.type == "map")
            {
                if (!section.id.empty())
                {
                    return failConfig(
                        error,
                        ConfigErrorCategory::Syntax,
                        sourcePath,
                        section.line,
                        "地图定义必须使用不带 ID 的 [map] 节");
                }
                if (mapSection != nullptr)
                {
                    return failConfig(
                        error,
                        ConfigErrorCategory::DuplicateDefinition,
                        sourcePath,
                        section.line,
                        "地图文件只能包含一个 [map] 节");
                }
                mapSection = &section;
                continue;
            }

            if (section.type == "route")
            {
                if (section.id.empty())
                {
                    return failConfig(
                        error,
                        ConfigErrorCategory::Syntax,
                        sourcePath,
                        section.line,
                        "路线必须使用 [route:id] 节并提供 ID");
                }
                routeSections.push_back(&section);
                continue;
            }

            return failConfig(
                error,
                ConfigErrorCategory::UnknownField,
                sourcePath,
                section.line,
                "地图文件包含未知节 [" + section.type + "]");
        }

        if (mapSection == nullptr)
        {
            return failConfig(
                error,
                ConfigErrorCategory::MissingField,
                sourcePath,
                0,
                "地图文件缺少 [map] 节");
        }
        if (routeSections.empty())
        {
            return failConfig(
                error,
                ConfigErrorCategory::MissingField,
                sourcePath,
                mapSection->line,
                "地图文件至少需要一条路线");
        }

        MapDefinition parsedMap;
        if (!readMapSection(*mapSection, sourcePath, parsedMap, error))
        {
            return false;
        }

        for (const ConfigSection* const section : routeSections)
        {
            Route route;
            if (!readRouteSection(*section, sourcePath, route, error))
            {
                return false;
            }
            parsedMap.routes.push_back(std::move(route));
        }

        // 此代码段在覆盖调用方输出前执行完整路线语义校验。
        if (!validateMapRoutes(
                parsedMap,
                *mapSection,
                routeSections,
                sourcePath,
                error))
        {
            return false;
        }

        map = std::move(parsedMap);
        return true;
    }
}
