#pragma once

#include <string>
#include <vector>

namespace autochess::core
{
    enum class MapSide
    {
        Unknown,
        A,
        B
    };

    struct GridPosition
    {
        int x = 0;
        int y = 0;
    };

    inline bool operator==(
        const GridPosition& left,
        const GridPosition& right) noexcept
    {
        return left.x == right.x && left.y == right.y;
    }

    inline bool operator!=(
        const GridPosition& left,
        const GridPosition& right) noexcept
    {
        return !(left == right);
    }

    inline bool operator<(
        const GridPosition& left,
        const GridPosition& right) noexcept
    {
        if (left.y != right.y)
        {
            return left.y < right.y;
        }

        return left.x < right.x;
    }

    struct Route
    {
        std::string id;
        MapSide side = MapSide::Unknown;
        GridPosition start;
        std::vector<GridPosition> points;
    };

    struct MapDefinition
    {
        std::string id;
        std::string name;
        int width = 0;
        int height = 0;
        std::vector<std::string> gridRows;
        std::vector<Route> routes;
    };
}
