#include "game/rendering/UnitRenderer.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>

#include <algorithm>
#include <cmath>

namespace autochess::game
{
    namespace
    {
        // 此函数为五种正式单位返回稳定类型色以便快速识别。
        sf::Color unitColor(const std::string& unitId, const sf::Uint8 alpha)
        {
            // 此代码块为防御、近战、远程、法术和医疗单位分配不同颜色。
            if (unitId == "training_guard")
            {
                return sf::Color(100, 125, 155, alpha);
            }
            if (unitId == "duelist")
            {
                return sf::Color(185, 115, 70, alpha);
            }
            if (unitId == "ranger")
            {
                return sf::Color(75, 155, 95, alpha);
            }
            if (unitId == "arcanist")
            {
                return sf::Color(135, 90, 180, alpha);
            }
            return sf::Color(75, 155, 155, alpha);
        }
    }

    // 此函数绘制内缩矩形、阵营边框和两行单位文字。
    void UnitRenderer::draw(
        sf::RenderTarget& target,
        const sf::Font& font,
        const sf::String& displayName,
        const core::UnitIdentity& identity,
        const core::MapSide side,
        const sf::FloatRect& bounds,
        const sf::Uint8 alpha)
    {
        const float inset = 5.0F;
        sf::RectangleShape body(sf::Vector2f(
            bounds.width - inset * 2.0F,
            bounds.height - inset * 2.0F));
        body.setPosition(bounds.left + inset, bounds.top + inset);
        body.setFillColor(unitColor(identity.unitId, alpha));
        body.setOutlineThickness(3.0F);
        body.setOutlineColor(
            side == core::MapSide::A
                ? sf::Color(95, 185, 255, alpha)
                : sf::Color(255, 115, 115, alpha));
        target.draw(body);

        // 此代码块把名称和等级组合为居中的两行短文本。
        sf::String label = displayName;
        label += L"\nLv.";
        label += sf::String(std::to_wstring(identity.level));
        sf::Text text(label, font, 15);
        text.setFillColor(sf::Color(250, 250, 250, alpha));
        const sf::FloatRect textBounds = text.getLocalBounds();
        text.setOrigin(
            textBounds.left + textBounds.width / 2.0F,
            textBounds.top + textBounds.height / 2.0F);
        text.setPosition(
            bounds.left + bounds.width / 2.0F,
            bounds.top + bounds.height / 2.0F);
        target.draw(text);
    }

    // 此函数将战斗实时属性压缩到固定尺寸的圆形和状态条中。
    void UnitRenderer::drawBattle(
        sf::RenderTarget& target,
        const sf::Font& font,
        const core::BattleUnit& unit,
        const sf::Vector2f center,
        const float radius,
        const float barWidth,
        const bool selected)
    {
        const float safeRadius = std::max(8.0F, radius);
        sf::CircleShape body(safeRadius);
        body.setOrigin(safeRadius, safeRadius);
        body.setPosition(center);
        body.setFillColor(unitColor(unit.identity.unitId, 255));
        body.setOutlineThickness(selected ? 4.0F : 2.0F);
        body.setOutlineColor(
            selected
                ? sf::Color(255, 225, 105)
                : (unit.side == core::MapSide::A
                       ? sf::Color(95, 185, 255)
                       : sf::Color(255, 115, 115)));
        target.draw(body);

        // 此代码块在单位圆心绘制短等级标记，避免地图上出现长文本。
        sf::Text levelText(
            sf::String(std::to_wstring(unit.identity.level)), font, 14);
        levelText.setFillColor(sf::Color(250, 250, 250));
        const sf::FloatRect levelBounds = levelText.getLocalBounds();
        levelText.setOrigin(
            levelBounds.left + levelBounds.width / 2.0F,
            levelBounds.top + levelBounds.height / 2.0F);
        levelText.setPosition(center);
        target.draw(levelText);

        // 此代码块绘制位于角色脚下的生命条和技力条，并限制比例范围。
        // 在上次位置基础上将整组状态条再下移 6 像素。
        const float safeBarWidth = std::max(24.0F, barWidth);
        const float barHeight = 4.0F;
        const auto ratio = [](const double current, const double maximum) {
            if (maximum <= 0.0)
            {
                return 0.0F;
            }
            return std::clamp(
                static_cast<float>(current / maximum), 0.0F, 1.0F);
        };
        const float healthRatio = ratio(unit.health, unit.stats.maxHealth);
        const float manaRatio = ratio(unit.currentMana, unit.maxMana);
        const sf::Vector2f barOrigin(
            center.x - safeBarWidth / 2.0F,
            center.y + safeRadius + 11.0F);

        sf::RectangleShape healthBack(
            sf::Vector2f(safeBarWidth, barHeight));
        healthBack.setPosition(barOrigin);
        healthBack.setFillColor(sf::Color(45, 45, 50));
        target.draw(healthBack);
        sf::RectangleShape healthFill(
            sf::Vector2f(safeBarWidth * healthRatio, barHeight));
        healthFill.setPosition(barOrigin);
        healthFill.setFillColor(sf::Color(90, 220, 120));
        target.draw(healthFill);

        const sf::Vector2f manaOrigin(
            center.x - safeBarWidth / 2.0F,
            barOrigin.y + barHeight + 2.0F);
        sf::RectangleShape manaBack(sf::Vector2f(safeBarWidth, barHeight));
        manaBack.setPosition(manaOrigin);
        manaBack.setFillColor(sf::Color(45, 45, 50));
        target.draw(manaBack);
        sf::RectangleShape manaFill(
            sf::Vector2f(safeBarWidth * manaRatio, barHeight));
        manaFill.setPosition(manaOrigin);
        manaFill.setFillColor(sf::Color(95, 175, 255));
        target.draw(manaFill);
    }
}
