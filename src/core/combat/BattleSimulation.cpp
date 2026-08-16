#include "core/combat/BattleSimulation.hpp"

#include "core/combat/CombatRules.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace autochess::core
{
    namespace
    {
        constexpr double kPositionEpsilon = 1.0e-9;
        constexpr double kRouteSegmentEpsilon = 1.0e-7;

        double squaredDistance(
            const BattlePosition left,
            const BattlePosition right) noexcept
        {
            const double deltaX = left.x - right.x;
            const double deltaY = left.y - right.y;
            return deltaX * deltaX + deltaY * deltaY;
        }

        double distanceToSegment(
            const BattlePosition point,
            const BattlePosition segmentStart,
            const BattlePosition segmentEnd) noexcept
        {
            const double directionX = segmentEnd.x - segmentStart.x;
            const double directionY = segmentEnd.y - segmentStart.y;
            const double lengthSquared =
                directionX * directionX + directionY * directionY;
            if (lengthSquared <= kPositionEpsilon)
            {
                return std::sqrt(squaredDistance(point, segmentStart));
            }

            const double pointX = point.x - segmentStart.x;
            const double pointY = point.y - segmentStart.y;
            const double projection = std::clamp(
                (pointX * directionX + pointY * directionY)
                    / lengthSquared,
                0.0,
                1.0);
            const BattlePosition closest{
                segmentStart.x + projection * directionX,
                segmentStart.y + projection * directionY};
            return std::sqrt(squaredDistance(point, closest));
        }
    }

    BattleSimulation::BattleSimulation(
        std::vector<BattleUnit> units,
        MapDefinition map,
        const int timeoutSeconds)
        : units_(std::move(units)),
          map_(std::move(map)),
          timeoutFrames_(timeoutSeconds <= 0
                  ? 0
                  : static_cast<std::uint64_t>(timeoutSeconds) * 60),
          summary_()
    {
        if (units_.empty())
        {
            finish(BattleSummary::EndReason::AllUnitsResolved);
        }
    }

    BattlePosition BattleSimulation::centerOf(
        const GridPosition position) noexcept
    {
        return BattlePosition{
            static_cast<double>(position.x) + 0.5,
            static_cast<double>(position.y) + 0.5};
    }

    bool BattleSimulation::isWalkable(
        const BattlePosition position) const noexcept
    {
        const int gridX = static_cast<int>(std::floor(position.x));
        const int gridY = static_cast<int>(std::floor(position.y));
        if (gridX < 0 || gridY < 0
            || gridX >= map_.width || gridY >= map_.height
            || gridY >= static_cast<int>(map_.gridRows.size())
            || gridX >= static_cast<int>(map_.gridRows[gridY].size()))
        {
            return false;
        }

        return map_.gridRows[gridY][gridX] != '#';
    }

    bool BattleSimulation::isOnCurrentRouteSegment(
        const BattleUnit& unit) const noexcept
    {
        if (unit.routePoints.empty()
            || unit.nextRoutePointIndex >= unit.routePoints.size())
        {
            return true;
        }

        if (unit.nextRoutePointIndex == 0)
        {
            return CombatRules::distance(
                       unit.position,
                       centerOf(unit.routePoints.front()))
                <= kRouteSegmentEpsilon;
        }

        const BattlePosition segmentStart = centerOf(
            unit.routePoints[unit.nextRoutePointIndex - 1]);
        const BattlePosition segmentEnd = centerOf(
            unit.routePoints[unit.nextRoutePointIndex]);
        return distanceToSegment(
                   unit.position,
                   segmentStart,
                   segmentEnd)
            <= kRouteSegmentEpsilon;
    }

    void BattleSimulation::recoverRouteProgress(
        BattleUnit& unit) const noexcept
    {
        if (isOnCurrentRouteSegment(unit)
            || unit.nextRoutePointIndex >= unit.routePoints.size())
        {
            return;
        }

        std::size_t nearestIndex = unit.nextRoutePointIndex;
        double nearestDistance = std::numeric_limits<double>::max();
        for (std::size_t index = unit.nextRoutePointIndex;
             index < unit.routePoints.size();
             ++index)
        {
            const double candidateDistance = squaredDistance(
                unit.position,
                centerOf(unit.routePoints[index]));
            if (candidateDistance < nearestDistance)
            {
                nearestDistance = candidateDistance;
                nearestIndex = index;
            }
        }

        unit.nextRoutePointIndex = nearestIndex;
    }

    void BattleSimulation::markReachedGuard(BattleUnit& unit) noexcept
    {
        unit.state = BattleUnitState::ReachedGuard;
        unit.targetId.reset();
        unit.firstInRangeFrame.clear();

        if (unit.side == MapSide::A)
        {
            summary_.guardDamageToB += unit.stats.guardDamage;
        }
        else if (unit.side == MapSide::B)
        {
            summary_.guardDamageToA += unit.stats.guardDamage;
        }
    }

    void BattleSimulation::moveUnit(BattleUnit& unit)
    {
        if (unit.state != BattleUnitState::Alive
            || unit.routePoints.empty())
        {
            return;
        }

        if (unit.nextRoutePointIndex >= unit.routePoints.size())
        {
            markReachedGuard(unit);
            return;
        }

        recoverRouteProgress(unit);
        double remainingDistance = std::max(
            0.0,
            unit.stats.moveSpeed * FixedDeltaSeconds);
        while (remainingDistance > kPositionEpsilon
            && unit.nextRoutePointIndex < unit.routePoints.size())
        {
            const BattlePosition target = centerOf(
                unit.routePoints[unit.nextRoutePointIndex]);
            const double distance = CombatRules::distance(
                unit.position,
                target);
            if (distance <= kPositionEpsilon)
            {
                unit.position = target;
                ++unit.nextRoutePointIndex;
                continue;
            }

            const double travelDistance =
                std::min(remainingDistance, distance);
            const double ratio = travelDistance / distance;
            const BattlePosition candidate{
                unit.position.x + (target.x - unit.position.x) * ratio,
                unit.position.y + (target.y - unit.position.y) * ratio};
            if (!isWalkable(candidate))
            {
                break;
            }

            unit.position = candidate;
            remainingDistance -= travelDistance;
            if (travelDistance + kPositionEpsilon >= distance)
            {
                unit.position = target;
                ++unit.nextRoutePointIndex;
            }
        }

        if (unit.nextRoutePointIndex >= unit.routePoints.size())
        {
            markReachedGuard(unit);
        }
    }

    bool BattleSimulation::allUnitsResolved() const noexcept
    {
        return std::all_of(
            units_.begin(),
            units_.end(),
            [](const BattleUnit& unit)
            {
                return unit.state != BattleUnitState::Alive;
            });
    }

    void BattleSimulation::finish(
        const BattleSummary::EndReason reason) noexcept
    {
        finished_ = true;
        summary_.endReason = reason;
        summary_.elapsedFrames = currentFrame_;
        summary_.elapsedSeconds =
            static_cast<double>(currentFrame_) * FixedDeltaSeconds;
        summary_.deadOwnedUnitIds.clear();
        summary_.reachedGuardOwnedUnitIds.clear();
        summary_.survivingOwnedUnitIds.clear();

        for (const BattleUnit& unit : units_)
        {
            if (unit.state == BattleUnitState::Dead)
            {
                summary_.deadOwnedUnitIds.push_back(unit.ownedUnitId);
            }
            else if (unit.state == BattleUnitState::ReachedGuard)
            {
                summary_.reachedGuardOwnedUnitIds.push_back(
                    unit.ownedUnitId);
            }
            else
            {
                summary_.survivingOwnedUnitIds.push_back(unit.ownedUnitId);
            }
        }
    }

    bool BattleSimulation::step()
    {
        if (finished_)
        {
            return false;
        }

        if (timeoutFrames_ == 0 || currentFrame_ >= timeoutFrames_)
        {
            finish(BattleSummary::EndReason::Timeout);
            return false;
        }

        ++currentFrame_;
        for (BattleUnit& unit : units_)
        {
            if (unit.state == BattleUnitState::Alive
                && !unit.targetId.has_value())
            {
                moveUnit(unit);
            }
        }

        if (allUnitsResolved())
        {
            finish(BattleSummary::EndReason::AllUnitsResolved);
        }
        else if (currentFrame_ >= timeoutFrames_)
        {
            finish(BattleSummary::EndReason::Timeout);
        }

        return true;
    }

    bool BattleSimulation::isFinished() const noexcept
    {
        return finished_;
    }

    std::uint64_t BattleSimulation::currentFrame() const noexcept
    {
        return currentFrame_;
    }

    const std::vector<BattleUnit>& BattleSimulation::units() const noexcept
    {
        return units_;
    }

    const BattleSummary& BattleSimulation::summary() const noexcept
    {
        return summary_;
    }
}
