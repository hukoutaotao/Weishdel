#pragma once

#include "core/config/ConfigBundleLoader.hpp"
#include "core/match/GameCommand.hpp"
#include "core/match/ReadOnlyGameView.hpp"

#include <optional>

// 此命名空间保存不依赖 SFML 的电脑策略抽象。
namespace autochess::core
{
    // 此接口规定策略只能从只读快照生成统一命令。
    class IAiStrategy
    {
    public:
        virtual ~IAiStrategy() = default;

        // 此函数返回策略枚举，供控制器和测试识别当前策略。
        virtual AiStrategyKind kind() const noexcept = 0;

        // 此函数返回游戏运行时默认使用的分队 ID。
        virtual const char* defaultFactionId() const noexcept = 0;

        // 此函数根据准备快照生成至多一条购买或部署命令。
        virtual std::optional<GameCommand> choosePreparationCommand(
            MapSide actor,
            const ReadOnlyGameView& view,
            const ConfigBundle& config) const = 0;

    };
}
