#pragma once

#include "core/combat/BattleTypes.hpp"
#include "core/map/MapTypes.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace autochess::core
{
    class BattleSimulation
    {
    public:
        static constexpr double FixedDeltaSeconds = 1.0 / 60.0;

        BattleSimulation(
            std::vector<BattleUnit> units,
            MapDefinition map,
            int timeoutSeconds);

        // 执行一个固定模拟帧；战斗结束后再次调用不会修改状态。
        bool step();

        bool isFinished() const noexcept;

        std::uint64_t currentFrame() const noexcept;

        const std::vector<BattleUnit>& units() const noexcept;

        const BattleSummary& summary() const noexcept;

    private:
        static BattlePosition centerOf(GridPosition position) noexcept;

        bool isWalkable(BattlePosition position) const noexcept;

        bool isOnCurrentRouteSegment(const BattleUnit& unit) const noexcept;

        void recoverRouteProgress(BattleUnit& unit) const noexcept;

        void moveUnit(BattleUnit& unit);

        void clearInvalidTarget(BattleUnit& unit) const noexcept;

        void updateTargets();

        void applyAttacks();

        void markReachedGuard(BattleUnit& unit) noexcept;

        bool allUnitsResolved() const noexcept;

        void finish(BattleSummary::EndReason reason) noexcept;

        std::vector<BattleUnit> units_;
        MapDefinition map_;
        std::uint64_t timeoutFrames_ = 0;
        std::uint64_t currentFrame_ = 0;
        bool finished_ = false;
        BattleSummary summary_;
    };
}
