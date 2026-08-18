#pragma once

#include "core/match/ReadOnlyGameView.hpp"
#include "core/config/ConfigBundleLoader.hpp"
#include "game/rendering/BoardTransform.hpp"
#include "game/screens/Screen.hpp"
#include "game/ui/Button.hpp"

#include <SFML/Graphics/Text.hpp>
#include <SFML/System/Clock.hpp>

#include <memory>
#include <string>
#include <vector>

namespace autochess::game
{
    // 此页面根据核心阶段显示地图、分队、AI 选择或准备占位内容。
    class MatchScreen final : public Screen
    {
    public:
        MatchScreen(
            const sf::Font& font,
            const core::ConfigBundle& config);

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

        struct ShopCard
        {
            std::unique_ptr<Button> button;
            std::size_t slot = 0;
        };

        // 此函数为当前核心阶段重建标题、说明和全部选项按钮。
        void rebuildChoices();

        // 此函数把一个选择项转换为当前阶段对应的 A 方命令。
        void submitChoice(const Choice& choice);

        // 此函数根据最新快照同步 HUD、商店标题和按钮可用状态。
        void refreshPreparationWidgets();

        // 此函数按单位 ID 查询配置中的中文名称并保留 ID 回退。
        sf::String unitName(const std::string& unitId) const;

        const sf::Font& font_;
        const core::ConfigBundle& config_;
        core::ReadOnlyGameView view_;
        core::MatchPhase builtPhase_ = core::MatchPhase::MatchResult;
        sf::Text title_;
        sf::Text hint_;
        sf::Text hud_;
        sf::Text shopTitle_;
        sf::Text message_;
        std::vector<Choice> choices_;
        std::vector<ShopCard> shopCards_;
        BoardTransform boardTransform_;
        std::unique_ptr<Button> routeToggleButton_;
        std::unique_ptr<Button> refreshButton_;
        bool showRoutes_ = false;
        sf::Clock messageClock_;
        UiAction pendingAction_;
    };
}
