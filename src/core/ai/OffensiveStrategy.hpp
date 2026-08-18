#pragma once

#include "core/ai/IAiStrategy.hpp"

// 此命名空间声明偏好攻击属性和短路线的电脑策略。
namespace autochess::core
{
    // 此类按攻击力、攻速和守卫伤害对商品和技能排序。
    class OffensiveStrategy final : public IAiStrategy
    {
    public:
        // 此函数返回进攻型策略枚举。
        AiStrategyKind kind() const noexcept override;

        // 此函数返回进攻型策略的默认分队。
        const char* defaultFactionId() const noexcept override;

        // 此函数生成进攻型准备阶段购买或部署命令。
        std::optional<GameCommand> choosePreparationCommand(
            MapSide actor,
            const ReadOnlyGameView& view,
            const ConfigBundle& config) const override;

        // 此函数生成进攻型战斗阶段技能命令。
        std::optional<GameCommand> chooseCombatCommand(
            MapSide actor,
            const ReadOnlyGameView& view,
            const ConfigBundle& config) const override;
    };
}
