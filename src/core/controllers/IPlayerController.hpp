#pragma once

#include "core/match/GameCommand.hpp"
#include "core/match/ReadOnlyGameView.hpp"

#include <optional>

// 此命名空间声明真人、脚本和未来 AI 控制器共用的抽象接口。
namespace autochess::core
{
    // 此接口只读取快照并返回命令，从而禁止控制器直接修改对局状态。
    class IPlayerController
    {
    public:
        virtual ~IPlayerController() = default;

        // 此函数返回控制器负责的固定阵营。
        virtual MapSide side() const noexcept = 0;

        // 此函数根据最新只读快照生成至多一条命令。
        virtual std::optional<GameCommand> nextCommand(
            const ReadOnlyGameView& view) = 0;
    };
}
