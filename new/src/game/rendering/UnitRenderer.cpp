#include "game/rendering/UnitRenderer.hpp"

#include "game/ui/UiTheme.hpp"

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
                return sf::Color(91, 100, 110, alpha);
            }
            if (unitId == "duelist")
            {
                return sf::Color(119, 91, 70, alpha);
            }
            if (unitId == "ranger")
            {
                return sf::Color(70, 111, 84, alpha);
            }
            if (unitId == "arcanist")
            {
                return sf::Color(102, 80, 121, alpha);
            }
            return sf::Color(67, 106, 106, alpha);
        }
    }

    // 此函数绘制内缩矩形、阵营边框和两行单位文字。
    void UnitRenderer::draw(
        sf::RenderTarget& target,
        const sf::Font& font,
        const sf::Font& englishFont,
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
                ? ui::withAlpha(ui::FriendlyStrong, alpha)
                : ui::withAlpha(ui::EnemyStrong, alpha));
        target.draw(body);

        sf::Text nameText(displayName, font);
        ui::setTextSize(nameText, 15);
        nameText.setFillColor(ui::withAlpha(ui::TextPrimary, alpha));
        ui::centerText(
            nameText,
            sf::FloatRect(bounds.left, bounds.top + 16.0F, bounds.width, 24.0F));
        target.draw(nameText);

        sf::Text levelText(
            sf::String(L"Lv. ") + sf::String(std::to_wstring(identity.level)),
            englishFont);
        ui::setTextSize(levelText, 14);
        levelText.setFillColor(ui::withAlpha(ui::TextSecondary, alpha));
        ui::centerText(
            levelText,
            sf::FloatRect(bounds.left, bounds.top + 40.0F, bounds.width, 22.0F));
        target.draw(levelText);
    }

    // 此函数将战斗实时属性压缩到固定尺寸的圆形和状态条中。
    void UnitRenderer::drawBattle(
        sf::RenderTarget& target,
        const sf::Font& englishFont,
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
                       ? ui::FriendlyStrong
                       : ui::EnemyStrong));
        target.draw(body);

        // 此代码块在单位圆心绘制短等级标记，避免地图上出现长文本。
        sf::Text levelText(
            sf::String(std::to_wstring(unit.identity.level)), englishFont);
        ui::setTextSize(levelText, 14);
        levelText.setFillColor(ui::TextPrimary);
        const sf::FloatRect levelBounds = levelText.getLocalBounds();
        levelText.setOrigin(
            levelBounds.left + levelBounds.width / 2.0F,
            levelBounds.top + levelBounds.height / 2.0F);
        levelText.setPosition(center);
        target.draw(levelText);

        // 此代码块绘制位于角色脚下的生命条，并限制比例范围。
        // 在上次位置基础上将整组状态条上移 10 像素。
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
        const sf::Vector2f barOrigin(
            center.x - safeBarWidth / 2.0F,
            center.y + safeRadius + 31.0F);

        sf::RectangleShape healthBack(
            sf::Vector2f(safeBarWidth, barHeight));
        healthBack.setPosition(barOrigin);
        healthBack.setFillColor(ui::Control);
        target.draw(healthBack);
        sf::RectangleShape healthFill(
            sf::Vector2f(safeBarWidth * healthRatio, barHeight));
        healthFill.setPosition(barOrigin);
        healthFill.setFillColor(ui::Health);
        target.draw(healthFill);

    }
}
