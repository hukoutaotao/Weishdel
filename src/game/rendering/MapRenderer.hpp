#pragma once

#include "core/map/MapTypes.hpp"
#include "game/rendering/BoardTransform.hpp"

#include <SFML/Graphics/RenderTarget.hpp>

namespace autochess::game
{
    // 此无状态绘制器使用色块表现地形并可选显示双方预定义路线。
    class MapRenderer
    {
    public:
        // 此函数绘制地图格子、外框以及可选路线调试层。
        static void draw(
            sf::RenderTarget& target,
            const core::MapDefinition& map,
            const BoardTransform& transform,
            bool showRoutes);
    };
}
