#include "core/model/PlayerStateService.hpp"

#include <cassert>
#include <set>

namespace autochess::core
{
    namespace
    {
        bool fail(std::string& errorMessage, const char* message)
        {
            errorMessage = message;
            return false;
        }
    }

    PlayerState PlayerStateService::createInitial(
        const MapSide side,
        const GameConfig& gameConfig,
        const FactionDefinition& faction)
    {
        assert(side != MapSide::Unknown);
        assert(gameConfig.rosterCapacity >= 0);

        PlayerState player;
        player.side = side;
        player.factionId = faction.id;
        player.gold = gameConfig.startingGold;
        player.guardValue = faction.initialGuard;
        player.reserveSlots.resize(
            static_cast<std::size_t>(gameConfig.rosterCapacity));
        return player;
    }

    const OwnedUnit* PlayerStateService::findActive(
        const PlayerState& player,
        const OwnedUnitId unitId) noexcept
    {
        for (const OwnedUnit& unit : player.activeUnits)
        {
            if (unit.id == unitId)
            {
                return &unit;
            }
        }

        return nullptr;
    }

    const OwnedUnit* PlayerStateService::findDead(
        const PlayerState& player,
        const OwnedUnitId unitId) noexcept
    {
        for (const OwnedUnit& unit : player.deadUnits)
        {
            if (unit.id == unitId)
            {
                return &unit;
            }
        }

        return nullptr;
    }

    std::optional<std::size_t> PlayerStateService::findReserveSlot(
        const PlayerState& player,
        const OwnedUnitId unitId) noexcept
    {
        for (std::size_t slot = 0; slot < player.reserveSlots.size(); ++slot)
        {
            if (player.reserveSlots[slot].has_value()
                && player.reserveSlots[slot].value() == unitId)
            {
                return slot;
            }
        }

        return std::nullopt;
    }

    std::optional<GridPosition> PlayerStateService::findDeploymentPosition(
        const PlayerState& player,
        const OwnedUnitId unitId) noexcept
    {
        for (const auto& deployment : player.deployments)
        {
            if (deployment.second == unitId)
            {
                return deployment.first;
            }
        }

        return std::nullopt;
    }

    std::optional<std::size_t>
        PlayerStateService::findFirstEmptyReserveSlot(
            const PlayerState& player) noexcept
    {
        for (std::size_t slot = 0; slot < player.reserveSlots.size(); ++slot)
        {
            if (!player.reserveSlots[slot].has_value())
            {
                return slot;
            }
        }

        return std::nullopt;
    }

    std::size_t PlayerStateService::activeCount(
        const PlayerState& player) noexcept
    {
        return player.activeUnits.size();
    }

    std::size_t PlayerStateService::deployedCount(
        const PlayerState& player) noexcept
    {
        return player.deployments.size();
    }

    bool PlayerStateService::validate(
        const PlayerState& player,
        std::string& errorMessage)
    {
        errorMessage.clear();

        if (player.side == MapSide::Unknown)
        {
            return fail(errorMessage, "玩家阵营不能是未知阵营");
        }

        if (player.gold < 0)
        {
            return fail(errorMessage, "玩家金币不能为负数");
        }

        if (player.guardValue < 0)
        {
            return fail(errorMessage, "玩家守卫值不能为负数");
        }

        if (player.activeUnits.size() > player.reserveSlots.size())
        {
            return fail(errorMessage, "活动单位数量超过持有上限");
        }

        std::set<OwnedUnitId> activeIds;
        for (const OwnedUnit& unit : player.activeUnits)
        {
            if (unit.id == InvalidOwnedUnitId)
            {
                return fail(errorMessage, "活动单位包含无效持久 ID");
            }

            if (unit.identity.unitId.empty())
            {
                return fail(errorMessage, "活动单位类型 ID 不能为空");
            }

            if (unit.identity.level < 1)
            {
                return fail(errorMessage, "活动单位等级必须至少为 1");
            }

            if (unit.ownerSide != player.side)
            {
                return fail(errorMessage, "活动单位所属阵营与玩家不一致");
            }

            if (!activeIds.insert(unit.id).second)
            {
                return fail(errorMessage, "活动单位存在重复持久 ID");
            }
        }

        std::set<OwnedUnitId> deadIds;
        for (const OwnedUnit& unit : player.deadUnits)
        {
            if (unit.id == InvalidOwnedUnitId)
            {
                return fail(errorMessage, "死亡单位包含无效持久 ID");
            }

            if (unit.identity.unitId.empty())
            {
                return fail(errorMessage, "死亡单位类型 ID 不能为空");
            }

            if (unit.identity.level < 1)
            {
                return fail(errorMessage, "死亡单位等级必须至少为 1");
            }

            if (unit.ownerSide != player.side)
            {
                return fail(errorMessage, "死亡单位所属阵营与玩家不一致");
            }

            if (!deadIds.insert(unit.id).second)
            {
                return fail(errorMessage, "死亡单位存在重复持久 ID");
            }

            if (activeIds.find(unit.id) != activeIds.end())
            {
                return fail(errorMessage, "单位不能同时处于活动和死亡列表");
            }
        }

        std::set<OwnedUnitId> locatedIds;
        for (const std::optional<OwnedUnitId>& slot : player.reserveSlots)
        {
            if (!slot.has_value())
            {
                continue;
            }

            const OwnedUnitId unitId = slot.value();
            if (unitId == InvalidOwnedUnitId)
            {
                return fail(errorMessage, "备用区包含无效持久 ID");
            }

            if (activeIds.find(unitId) == activeIds.end())
            {
                return fail(errorMessage, "备用区引用了不存在的活动单位");
            }

            if (!locatedIds.insert(unitId).second)
            {
                return fail(errorMessage, "活动单位在位置容器中重复出现");
            }
        }

        for (const auto& deployment : player.deployments)
        {
            const OwnedUnitId unitId = deployment.second;
            if (unitId == InvalidOwnedUnitId)
            {
                return fail(errorMessage, "部署映射包含无效持久 ID");
            }

            if (activeIds.find(unitId) == activeIds.end())
            {
                return fail(errorMessage, "部署映射引用了不存在的活动单位");
            }

            if (!locatedIds.insert(unitId).second)
            {
                return fail(errorMessage, "活动单位在位置容器中重复出现");
            }
        }

        if (locatedIds.size() != activeIds.size())
        {
            return fail(errorMessage, "活动单位没有恰好一个位置");
        }

        for (const OwnedUnitId unitId : deadIds)
        {
            if (locatedIds.find(unitId) != locatedIds.end())
            {
                return fail(errorMessage, "死亡单位不能出现在位置容器中");
            }
        }

        return true;
    }
}
