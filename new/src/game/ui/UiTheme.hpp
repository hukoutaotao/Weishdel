#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Rect.hpp>

#include <cmath>

namespace autochess::game::ui
{
    // 这组颜色取自参考界面的深灰层级，并为棋盘双方保留低饱和度识别色。
    inline const sf::Color Background(17, 17, 17);
    inline const sf::Color Surface(22, 22, 22);
    inline const sf::Color Panel(29, 29, 29);
    inline const sf::Color Control(39, 39, 39);
    inline const sf::Color ControlHover(49, 49, 49);
    inline const sf::Color ControlPressed(58, 58, 58);
    inline const sf::Color ControlDisabled(31, 31, 31);
    inline const sf::Color Border(55, 55, 55);
    inline const sf::Color BorderStrong(78, 78, 78);
    inline const sf::Color TextPrimary(242, 242, 242);
    inline const sf::Color TextSecondary(177, 177, 177);
    inline const sf::Color TextMuted(119, 119, 119);
    inline const sf::Color Success(91, 190, 143);
    inline const sf::Color Error(220, 105, 113);
    inline const sf::Color Friendly(72, 102, 139);
    inline const sf::Color FriendlyStrong(88, 132, 184);
    inline const sf::Color Enemy(128, 66, 72);
    inline const sf::Color EnemyStrong(173, 82, 91);
    inline const sf::Color Health(90, 205, 126);

    inline sf::Color withAlpha(const sf::Color& color, const sf::Uint8 alpha)
    {
        return sf::Color(color.r, color.g, color.b, alpha);
    }

    constexpr unsigned int TextRasterScale = 2;
    constexpr float TextDisplayScale = 1.0F / static_cast<float>(TextRasterScale);

    // 文字先以双倍字号栅格化，再缩回逻辑尺寸；窗口放大时不再放大低分辨率字形纹理。
    inline void setTextSize(sf::Text& text, const unsigned int logicalSize)
    {
        text.setCharacterSize(logicalSize * TextRasterScale);
        text.setScale(TextDisplayScale, TextDisplayScale);
        text.setStyle(text.getStyle() | sf::Text::Bold);
    }

    inline float snapToPhysicalPixel(const float logicalCoordinate)
    {
        return std::round(logicalCoordinate * TextRasterScale)
            / static_cast<float>(TextRasterScale);
    }

    // 把文字的真实可见边界精确放到目标矩形中心，并对齐到最终物理像素。
    inline void centerText(sf::Text& text, const sf::FloatRect& bounds)
    {
        const sf::FloatRect textBounds = text.getLocalBounds();
        text.setOrigin(
            textBounds.left + textBounds.width / 2.0F,
            textBounds.top + textBounds.height / 2.0F);
        text.setPosition(
            snapToPhysicalPixel(bounds.left + bounds.width / 2.0F),
            snapToPhysicalPixel(bounds.top + bounds.height / 2.0F));
    }

    // 仅水平居中并让文字可见上沿落在指定位置，适用于页面标题和说明。
    inline void centerTextAtTop(
        sf::Text& text,
        const float centerX,
        const float top)
    {
        const sf::FloatRect textBounds = text.getLocalBounds();
        text.setOrigin(
            textBounds.left + textBounds.width / 2.0F,
            textBounds.top);
        text.setPosition(
            snapToPhysicalPixel(centerX),
            snapToPhysicalPixel(top));
    }

    inline void placeTextAtTopLeft(
        sf::Text& text,
        const float left,
        const float top)
    {
        const sf::FloatRect textBounds = text.getLocalBounds();
        text.setOrigin(textBounds.left, textBounds.top);
        text.setPosition(
            snapToPhysicalPixel(left),
            snapToPhysicalPixel(top));
    }
}
