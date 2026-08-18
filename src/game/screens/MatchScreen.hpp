#pragma once

#include "core/match/ReadOnlyGameView.hpp"
#include "game/screens/Screen.hpp"
#include "game/ui/Button.hpp"

#include <SFML/Graphics/Text.hpp>

#include <memory>
#include <string>
#include <vector>

namespace autochess::game
{
    // 此页面根据核心阶段显示地图、分队、AI 选择或准备占位内容。
    class MatchScreen final : public Screen
    {
    public:
        explicit MatchScreen(const sf::Font& font);

        void handleEvent(const sf::Event& event) override;
        void draw(sf::RenderTarget& target) const override;
        UiAction takeAction() override;

        // 此函数复制最新只读快照并在阶段变化时重建选择按钮。
        void updateView(const core::ReadOnlyGameView& view);

        // 此函数显示最近一次核心命令的中文结果。
        void showCommandResult(const core::CommandResult& result);

    private:
        struct Choice
        {
            std::unique_ptr<Button> button;
            std::string id;
            core::AiStrategyKind strategy = core::AiStrategyKind::Unknown;
        };

        // 此函数为当前核心阶段重建标题、说明和全部选项按钮。
        void rebuildChoices();

        // 此函数把一个选择项转换为当前阶段对应的 A 方命令。
        void submitChoice(const Choice& choice);

        const sf::Font& font_;
        core::ReadOnlyGameView view_;
        core::MatchPhase builtPhase_ = core::MatchPhase::MatchResult;
        sf::Text title_;
        sf::Text hint_;
        sf::Text message_;
        std::vector<Choice> choices_;
        UiAction pendingAction_;
    };
}
