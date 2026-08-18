#include "game/rendering/UnitRenderer.hpp"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>

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
}
