#include "game/ui/Button.hpp"

#include <SFML/Window/Mouse.hpp>

namespace autochess::game
{
    // 此构造函数建立按钮几何、文字和统一边框样式。
    Button::Button(
        const sf::Font& font,
        const sf::FloatRect& bounds,
        const sf::String& label,
        const unsigned int characterSize)
    {
        shape_.setPosition(bounds.left, bounds.top);
        shape_.setSize(sf::Vector2f(bounds.width, bounds.height));
        shape_.setOutlineThickness(2.0F);
        shape_.setOutlineColor(sf::Color(120, 145, 190));

        label_.setFont(font);
        label_.setString(label);
        label_.setCharacterSize(characterSize);
        label_.setFillColor(sf::Color::White);
        refreshVisual();
    }

    // 此函数只把同一次左键按下和释放都落在按钮内视为点击。
    bool Button::handleEvent(const sf::Event& event)
    {
        // 此代码块在禁用状态清除交互状态且拒绝所有点击。
        if (!enabled_)
        {
            hovered_ = false;
            pressed_ = false;
            refreshVisual();
            return false;
        }

        // 此代码块根据鼠标移动位置更新悬停外观。
        if (event.type == sf::Event::MouseMoved)
        {
            hovered_ = shape_.getGlobalBounds().contains(
                static_cast<float>(event.mouseMove.x),
                static_cast<float>(event.mouseMove.y));
            refreshVisual();
        }

        // 此代码块仅在左键于按钮内按下时记录有效按压。
        if (event.type == sf::Event::MouseButtonPressed
            && event.mouseButton.button == sf::Mouse::Left)
        {
            pressed_ = shape_.getGlobalBounds().contains(
                static_cast<float>(event.mouseButton.x),
                static_cast<float>(event.mouseButton.y));
            refreshVisual();
        }

        // 此代码块在左键释放时生成至多一次完整点击结果。
        if (event.type == sf::Event::MouseButtonReleased
            && event.mouseButton.button == sf::Mouse::Left)
        {
            const bool releasedInside = shape_.getGlobalBounds().contains(
                static_cast<float>(event.mouseButton.x),
                static_cast<float>(event.mouseButton.y));
            const bool clicked = pressed_ && releasedInside;
            pressed_ = false;
            refreshVisual();
            return clicked;
        }

        return false;
    }

    // 此函数按背景、边框、标题顺序绘制一个完整按钮。
    void Button::draw(sf::RenderTarget& target) const
    {
        target.draw(shape_);
        target.draw(label_);
    }

    // 此函数切换可用状态并立即更新视觉状态。
    void Button::setEnabled(const bool enabled) noexcept
    {
        enabled_ = enabled;
        // 此代码块在禁用按钮时取消残留的按压和悬停状态。
        if (!enabled_)
        {
            hovered_ = false;
            pressed_ = false;
        }
        refreshVisual();
    }

    // 此函数更新按钮文字并保持几何中心不变。
    void Button::setLabel(const sf::String& label)
    {
        label_.setString(label);
        centerLabel();
    }

    // 此函数返回按钮背景的全局矩形供其他界面布局复用。
    sf::FloatRect Button::bounds() const noexcept
    {
        return shape_.getGlobalBounds();
    }

    // 此函数为禁用、按下、悬停和普通状态选择稳定颜色。
    void Button::refreshVisual()
    {
        // 此代码块按照交互优先级选择唯一背景颜色。
        if (!enabled_)
        {
            shape_.setFillColor(sf::Color(70, 74, 84));
            label_.setFillColor(sf::Color(145, 145, 155));
        }
        else if (pressed_)
        {
            shape_.setFillColor(sf::Color(55, 95, 155));
            label_.setFillColor(sf::Color::White);
        }
        else if (hovered_)
        {
            shape_.setFillColor(sf::Color(65, 105, 170));
            label_.setFillColor(sf::Color::White);
        }
        else
        {
            shape_.setFillColor(sf::Color(48, 62, 92));
            label_.setFillColor(sf::Color(235, 238, 248));
        }
        centerLabel();
    }

    // 此函数通过文字局部边界修正基线偏移后执行精确居中。
    void Button::centerLabel()
    {
        const sf::FloatRect textBounds = label_.getLocalBounds();
        label_.setOrigin(
            textBounds.left + textBounds.width / 2.0F,
            textBounds.top + textBounds.height / 2.0F);
        const sf::FloatRect buttonBounds = shape_.getGlobalBounds();
        label_.setPosition(
            buttonBounds.left + buttonBounds.width / 2.0F,
            buttonBounds.top + buttonBounds.height / 2.0F);
    }
}
