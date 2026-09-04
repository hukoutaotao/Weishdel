#pragma once

#include "core/map/MapTypes.hpp"
#include "core/model/Definitions.hpp"
#include "core/model/PlayerTypes.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace autochess::core
{
    using BattleUnitId = std::uint64_t;

    inline constexpr BattleUnitId InvalidBattleUnitId = 0;

    struct BattlePosition
    {
        double x = 0.0;
        double y = 0.0;
    };

    inline bool operator==(
        const BattlePosition& left,
        const BattlePosition& right) noexcept
    {
        return left.x == right.x && left.y == right.y;
    }

    struct BattleStats
    {
        double maxHealth = 0.0;
        double attackPower = 0.0;
        double physicalDefense = 0.0;
        double magicResistance = 0.0;
        double attackRange = 0.0;
        double moveSpeed = 0.0;
        double attackSpeed = 0.0;
        int guardDamage = 0;
    };

    enum class BattleUnitState
    {
        Alive,
        Dead,
        ReachedGuard
    };

    struct BattleUnit
    {
        BattleUnitId id = InvalidBattleUnitId;
        OwnedUnitId ownedUnitId = InvalidOwnedUnitId;
        UnitIdentity identity;
        MapSide side = MapSide::Unknown;
        BasicAction basicAction = BasicAction::Unknown;
        DamageType basicDamageType = DamageType::Unknown;
        BattleStats stats;

        BattlePosition position;
        std::vector<GridPosition> routePoints;
        std::size_t nextRoutePointIndex = 0;

        double health = 0.0;
        double basicActionElapsed = 0.0;
        // 此序号表示已成功完成一次普通攻击或治疗，供表现层可靠触发动作。
        std::uint64_t basicActionSequence = 0;
        std::optional<BattleUnitId> targetId;
        std::map<BattleUnitId, std::uint64_t> firstInRangeFrame;
        BattleUnitState state = BattleUnitState::Alive;
    };

    struct BattleSummary
    {
        enum class EndReason
        {
            Ongoing,
            AllUnitsResolved,
            Timeout
        };

        EndReason endReason = EndReason::Ongoing;
        std::uint64_t elapsedFrames = 0;
        double elapsedSeconds = 0.0;
        int guardDamageToA = 0;
        int guardDamageToB = 0;
        std::vector<OwnedUnitId> deadOwnedUnitIds;
        std::vector<OwnedUnitId> reachedGuardOwnedUnitIds;
        std::vector<OwnedUnitId> survivingOwnedUnitIds;
    };
}
