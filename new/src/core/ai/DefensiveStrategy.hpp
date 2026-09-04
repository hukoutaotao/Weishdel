#pragma once

#include "core/ai/IAiStrategy.hpp"

// 此命名空间声明偏好生命、防御和治疗能力的电脑策略。
namespace autochess::core
{
    // 此类按生存能力、医师职责和主要威胁路线进行决策。
    class DefensiveStrategy final : public IAiStrategy
    {
    public:
        // 此函数返回防守型策略枚举。
        AiStrategyKind kind() const noexcept override;

        // 此函数返回防守型策略的默认分队。
        const char* defaultFactionId() const noexcept override;

        // 此函数生成防守型准备阶段购买或部署命令。
        std::optional<GameCommand> choosePreparationCommand(
            MapSide actor,
            const ReadOnlyGameView& view,
            const ConfigBundle& config) const override;

    };
}
