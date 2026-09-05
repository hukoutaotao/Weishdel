#pragma once

#include "game/ui/UiAction.hpp"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Window/Event.hpp>

namespace autochess::game
{
    // 此抽象类统一所有页面的事件、绘制和动作读取接口。
    class Screen
    {
    public:
        virtual ~Screen() = default;

        // 此函数让当前页面处理一个 SFML 窗口事件。
        virtual void handleEvent(const sf::Event& event) = 0;

        // 此函数把当前页面完整绘制到指定渲染目标。
        virtual void draw(sf::RenderTarget& target) const = 0;

        // 此函数取出页面产生的一次动作并把内部动作清空。
        virtual UiAction takeAction() = 0;
    };
}
