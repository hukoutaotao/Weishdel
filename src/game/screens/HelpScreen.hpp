#pragma once

#include "game/screens/Screen.hpp"
#include "game/ui/Button.hpp"

#include <SFML/Graphics/Text.hpp>

namespace autochess::game
{
    // 此页面用中文说明第七天能够执行的鼠标准备操作。
    class HelpScreen final : public Screen
    {
    public:
        explicit HelpScreen(const sf::Font& font);

        void handleEvent(const sf::Event& event) override;
        void draw(sf::RenderTarget& target) const override;
        UiAction takeAction() override;

    private:
        sf::Text title_;
        sf::Text content_;
        Button backButton_;
        UiAction pendingAction_;
    };
}
