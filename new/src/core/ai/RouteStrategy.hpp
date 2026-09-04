#pragma once

#include "core/ai/IAiStrategy.hpp"

// 此命名空间声明根据路线长度和移动效率分配单位的电脑策略。
namespace autochess::core
{
    // 此类使用预计移动时间、射程和守卫伤害选择商品与路线。
    class RouteStrategy final : public IAiStrategy
    {
    public:
        // 此函数返回路线型策略枚举。
        AiStrategyKind kind() const noexcept override;

        // 此函数返回路线型策略的默认分队。
        const char* defaultFactionId() const noexcept override;

        // 此函数生成路线型准备阶段购买或部署命令。
        std::optional<GameCommand> choosePreparationCommand(
            MapSide actor,
            const ReadOnlyGameView& view,
            const ConfigBundle& config) const override;

    };
}
