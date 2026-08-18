#pragma once

#include "core/controllers/IPlayerController.hpp"

namespace autochess::game
{
    // 此占位控制器只负责第七天图形联调所需的选分队、购买和部署。
    class BootstrapComputerController final : public core::IPlayerController
    {
    public:
        core::MapSide side() const noexcept override;

        std::optional<core::GameCommand> nextCommand(
            const core::ReadOnlyGameView& view) override;

    private:
        // 此函数把玩家选择的策略映射到第七天固定电脑分队。
        static const char* factionFor(core::AiStrategyKind strategy) noexcept;
    };
}
