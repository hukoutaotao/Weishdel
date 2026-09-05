#pragma once

#include "core/controllers/IPlayerController.hpp"

#include <deque>

namespace autochess::game
{
    // 此控制器把鼠标界面产生的命令排队并按固定帧逐条交给核心。
    class HumanController final : public core::IPlayerController
    {
    public:
        explicit HumanController(core::MapSide side);

        core::MapSide side() const noexcept override;

        std::optional<core::GameCommand> nextCommand(
            const core::ReadOnlyGameView& view) override;

        // 此函数把一条界面命令加入先进先出队列。
        void enqueue(core::GameCommand command);

        // 此函数清除新对局开始前尚未执行的旧命令。
        void clear() noexcept;

    private:
        core::MapSide side_ = core::MapSide::Unknown;
        std::deque<core::GameCommand> commands_;
    };
}
