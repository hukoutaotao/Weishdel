#pragma once

#include "core/economy/EconomyTypes.hpp"
#include "core/model/Definitions.hpp"
#include "core/model/PlayerTypes.hpp"

#include <vector>

namespace autochess::core
{
    // 此服务负责准备阶段持久单位的出售、死亡转移和复活。
    class RosterService
    {
    public:
        // 出售活动单位并返还经分队修正后一级价格的配置比例。
        static CommandResult sell(
            PlayerState& player,
            OwnedUnitId unitId,
            const GameConfig& gameConfig,
            const std::vector<UnitDefinition>& units,
            const FactionDefinition& faction,
            const std::vector<FactionModifierDefinition>& modifiers);

        // 将活动单位从当前位置原子地转入死亡列表。
        static CommandResult markDead(
            PlayerState& player,
            OwnedUnitId unitId);

        // 支付复活费后将死亡单位放入第一个空闲备用区槽位。
        static CommandResult revive(
            PlayerState& player,
            OwnedUnitId unitId,
            const GameConfig& gameConfig,
            const std::vector<UnitDefinition>& units,
            const FactionDefinition& faction,
            const std::vector<FactionModifierDefinition>& modifiers);
    };
}
