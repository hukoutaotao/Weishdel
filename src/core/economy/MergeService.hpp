#pragma once

#include "core/economy/EconomyTypes.hpp"
#include "core/model/Definitions.hpp"
#include "core/model/PlayerTypes.hpp"

#include <vector>

namespace autochess::core
{
    // 此服务负责准备阶段的持久单位合成，不处理普通位置移动。
    class MergeService
    {
    public:
        // 消耗两个同类型同等级活动单位，并在目标单位原位置生成高一级单位。
        static CommandResult merge(
            PlayerState& player,
            OwnedUnitId sourceUnitId,
            OwnedUnitId targetUnitId,
            const GameConfig& gameConfig,
            const std::vector<UnitDefinition>& units,
            const FactionDefinition& faction,
            const std::vector<FactionModifierDefinition>& modifiers,
            OwnedUnitId& nextOwnedUnitId);
    };
}
