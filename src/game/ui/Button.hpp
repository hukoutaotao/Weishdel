#pragma once

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Window/Event.hpp>

namespace autochess::game
{
    // 此控件提供统一的鼠标悬停、按下、禁用和文字居中行为。
    class Button
    {
    public:
        Button(
            const sf::Font& font,
            const sf::FloatRect& bounds,
            const sf::String& label,
            unsigned int characterSize = 24);

        // 此函数更新按钮状态并在完整点击发生时返回真。
        bool handleEvent(const sf::Event& event);

        // 此函数绘制当前状态对应的按钮背景、边框和标题。
        void draw(sf::RenderTarget& target) const;

        // 此函数设置按钮是否接受点击并同步禁用外观。
        void setEnabled(bool enabled) noexcept;

        // 此函数替换按钮标题并重新执行文字居中。
        void setLabel(const sf::String& label);

        // 此函数返回按钮用于命中检测的固定矩形。
        sf::FloatRect bounds() const noexcept;

    private:
        // 此函数根据当前状态选择背景色并保持标题居中。
        void refreshVisual();

        // 此函数把标题中心与按钮矩形中心对齐。
        void centerLabel();

        sf::RectangleShape shape_;
        sf::Text label_;
        bool enabled_ = true;
        bool hovered_ = false;
        bool pressed_ = false;
    };
}
