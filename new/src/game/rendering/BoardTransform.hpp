#pragma once

#include "core/combat/BattleTypes.hpp"
#include "core/map/MapTypes.hpp"

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <optional>

namespace autochess::game
{
    // 此类型集中负责地图格子和窗口像素之间的双向换算。
    class BoardTransform
    {
    public:
        // 此函数根据地图尺寸把棋盘按比例居中到指定视口。
        void configure(
            const sf::FloatRect& viewport,
            int mapWidth,
            int mapHeight) noexcept;

        // 此函数返回当前是否已经配置为可用地图尺寸。
        bool valid() const noexcept;

        // 此函数返回指定地图格子的像素矩形。
        sf::FloatRect cellBounds(core::GridPosition position) const noexcept;

        // 此函数返回指定地图格子的像素中心。
        sf::Vector2f cellCenter(core::GridPosition position) const noexcept;

        // 此函数把战斗单位的连续格坐标转换为窗口像素中心。
        sf::Vector2f battlePositionToPixel(
            core::BattlePosition position) const noexcept;

        // 此函数把棋盘内鼠标像素转换为格子，棋盘外返回空值。
        std::optional<core::GridPosition> pixelToGrid(
            sf::Vector2f pixel) const noexcept;

        // 此函数返回地图实际占用的居中像素矩形。
        sf::FloatRect boardBounds() const noexcept;

    private:
        sf::Vector2f origin_;
        float tileSize_ = 0.0F;
        int mapWidth_ = 0;
        int mapHeight_ = 0;
    };
}
