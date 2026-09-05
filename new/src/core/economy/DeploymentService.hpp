#pragma once

#include "core/economy/EconomyTypes.hpp"
#include "core/map/MapTypes.hpp"
#include "core/model/Definitions.hpp"
#include "core/model/PlayerTypes.hpp"

#include <cstddef>

namespace autochess::core
{
    // 此服务只负责准备阶段的备用区和部署区位置变更，不处理合成。
    class DeploymentService
    {
    public:
        // 将活动单位移动到当前玩家合法且为空的部署起点。
        static CommandResult moveToDeployment(
            PlayerState& player,
            const MapDefinition& map,
            const FactionDefinition& faction,
            OwnedUnitId unitId,
            GridPosition target);

        // 将活动单位移动到指定且为空的备用区槽位。
        static CommandResult moveToReserve(
            PlayerState& player,
            OwnedUnitId unitId,
            std::size_t targetSlot);
    };
}
