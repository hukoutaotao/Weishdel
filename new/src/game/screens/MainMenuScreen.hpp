#pragma once

#include "game/screens/Screen.hpp"
#include "game/ui/Button.hpp"

#include <SFML/Graphics/Text.hpp>

namespace autochess::game
{
    // 此页面显示游戏标题并产生开始、帮助和退出三种导航动作。
    class MainMenuScreen final : public Screen
    {
    public:
        explicit MainMenuScreen(const sf::Font& font);

        void handleEvent(const sf::Event& event) override;
        void draw(sf::RenderTarget& target) const override;
        UiAction takeAction() override;

    private:
        sf::Text title_;
        Button startButton_;
        Button helpButton_;
        Button exitButton_;
        UiAction pendingAction_;
    };
}
