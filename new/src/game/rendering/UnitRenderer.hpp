#pragma once

#include "core/combat/BattleTypes.hpp"
#include "core/map/MapTypes.hpp"
#include "core/model/PlayerTypes.hpp"

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/String.hpp>

namespace autochess::game
{
    // 此无状态绘制器用色块、中文名和等级表示准备阶段单位。
    class UnitRenderer
    {
    public:
        // 此函数在给定矩形内绘制一个按阵营描边的单位标记。
        static void draw(
            sf::RenderTarget& target,
            const sf::Font& font,
            const sf::String& displayName,
            const core::UnitIdentity& identity,
            core::MapSide side,
            const sf::FloatRect& bounds,
            sf::Uint8 alpha = 255);

        // 此函数用圆形标记、等级和生命条绘制战斗单位。
        static void drawBattle(
            sf::RenderTarget& target,
            const sf::Font& font,
            const core::BattleUnit& unit,
            sf::Vector2f center,
            float radius,
            float barWidth,
            bool selected);
    };
}
