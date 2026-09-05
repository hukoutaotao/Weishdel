#pragma once

#include "core/map/MapTypes.hpp"
#include "game/rendering/BoardTransform.hpp"

#include <SFML/Graphics/RenderTarget.hpp>

#include <optional>

namespace autochess::game
{
    // 此无状态绘制器使用色块表现地形，并仅在拖拽悬停时显示对应路线。
    class MapRenderer
    {
    public:
        // 此函数绘制地图格子、外框以及当前拖拽格的路线预览。
        static void draw(
            sf::RenderTarget& target,
            const core::MapDefinition& map,
            const BoardTransform& transform,
            const std::optional<core::GridPosition>& deploymentPreview);
    };
}
