#pragma once

#include "core/combat/BattleTypes.hpp"
#include "core/map/MapTypes.hpp"
#include "core/model/Definitions.hpp"
#include "core/model/PlayerTypes.hpp"

#include <string>
#include <vector>

namespace autochess::core
{
    // 将准备阶段的部署映射转换为战斗阶段的临时单位，不修改玩家状态。
    class BattleSetupService
    {
    public:
        static bool createUnits(
            const PlayerState& playerA,
            const PlayerState& playerB,
            const MapDefinition& map,
            const std::vector<UnitDefinition>& definitions,
            std::vector<BattleUnit>& output,
            std::string& errorMessage);
    };
}
