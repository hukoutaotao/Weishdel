#pragma once

#include "core/map/MapTypes.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace autochess::core
{
    using OwnedUnitId = std::uint64_t;

    inline constexpr OwnedUnitId InvalidOwnedUnitId = 0;

    struct UnitIdentity
    {
        std::string unitId;
        int level = 0;
    };

    inline bool operator==(
        const UnitIdentity& left,
        const UnitIdentity& right) noexcept
    {
        return left.unitId == right.unitId && left.level == right.level;
    }

    struct OwnedUnit
    {
        OwnedUnitId id = InvalidOwnedUnitId;
        UnitIdentity identity;
        MapSide ownerSide = MapSide::Unknown;
    };

    struct PlayerState
    {
        MapSide side = MapSide::Unknown;
        std::string factionId;
        int gold = 0;
        int guardValue = 0;

        std::vector<OwnedUnit> activeUnits;
        std::vector<OwnedUnit> deadUnits;

        // 每个活动单位必须恰好出现在备用区或部署区之一。
        std::vector<std::optional<OwnedUnitId>> reserveSlots;
        std::map<GridPosition, OwnedUnitId> deployments;

        // 死亡单位不得出现在位置容器中，也不计入非死亡单位持有上限。
    };
}
