#include "core/combat/BattleSimulation.hpp"

#include "core/combat/CombatRules.hpp"
#include "core/combat/TargetSelector.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

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
            frameGuardDamageToB_ += unit.stats.guardDamage;
        }
        else if (unit.side == MapSide::B)
        {
            frameGuardDamageToA_ += unit.stats.guardDamage;
        }
    }

    void BattleSimulation::applyFrameGuardDamage() noexcept
    {
        summary_.guardDamageToA += frameGuardDamageToA_;
        summary_.guardDamageToB += frameGuardDamageToB_;
        frameGuardDamageToA_ = 0;
        frameGuardDamageToB_ = 0;
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

    void BattleSimulation::clearInvalidTarget(
        BattleUnit& unit) const noexcept
    {
        if (!unit.targetId.has_value())
        {
            return;
        }

        const auto iterator = std::find_if(
            units_.begin(),
            units_.end(),
            [&unit](const BattleUnit& candidate)
            {
                return candidate.id == unit.targetId.value();
            });
        if (iterator == units_.end()
            || iterator->state != BattleUnitState::Alive
            || iterator->side == unit.side
            || CombatRules::distance(
                   unit.position,
                   iterator->position)
                > unit.stats.attackRange)
        {
            unit.targetId.reset();
        }
    }

    void BattleSimulation::updateTargets()
    {
        for (BattleUnit& unit : units_)
        {
            if (unit.state == BattleUnitState::Alive)
            {
                TargetSelector::update(unit, units_, currentFrame_);
            }
        }
    }

    void BattleSimulation::applyAttacks()
    {
        struct AttackIntent
        {
            BattleUnitId attackerId = InvalidBattleUnitId;
            BattleUnitId targetId = InvalidBattleUnitId;
            double damage = 0.0;
        };

        std::vector<AttackIntent> intents;
        for (BattleUnit& attacker : units_)
        {
            if (attacker.state != BattleUnitState::Alive
                || attacker.basicAction != BasicAction::Attack
                || !attacker.targetId.has_value())
            {
                continue;
            }

            const double interval = CombatRules::attackInterval(
                attacker.stats.attackSpeed);
            if (attacker.attackElapsed + 1.0e-9 < interval)
            {
                continue;
            }

            const auto targetIterator = std::find_if(
                units_.begin(),
                units_.end(),
                [&attacker](const BattleUnit& candidate)
                {
                    return candidate.id == attacker.targetId.value()
                        && candidate.state == BattleUnitState::Alive
                        && candidate.side != attacker.side;
                });
            if (targetIterator == units_.end())
            {
                attacker.targetId.reset();
                continue;
            }

            double damage = 0.0;
            if (attacker.basicDamageType == DamageType::Physical)
            {
                damage = CombatRules::physicalDamage(
                    attacker.stats.attackPower,
                    targetIterator->stats.physicalDefense);
            }
            else if (attacker.basicDamageType == DamageType::Magic)
            {
                damage = CombatRules::magicDamage(
                    attacker.stats.attackPower,
                    targetIterator->stats.magicResistance);
            }
            else
            {
                continue;
            }

            intents.push_back(AttackIntent{
                attacker.id,
                targetIterator->id,
                damage});
            attacker.attackElapsed = 0.0;
        }

        std::map<BattleUnitId, double> accumulatedDamage;
        for (const AttackIntent& intent : intents)
        {
            accumulatedDamage[intent.targetId] += intent.damage;
        }

        for (const auto& damage : accumulatedDamage)
        {
            const auto targetIterator = std::find_if(
                units_.begin(),
                units_.end(),
                [&damage](const BattleUnit& unit)
                {
                    return unit.id == damage.first
                        && unit.state == BattleUnitState::Alive;
                });
            if (targetIterator == units_.end())
            {
                continue;
            }

            targetIterator->health -= damage.second;
            if (targetIterator->health <= 0.0)
            {
                targetIterator->health = 0.0;
                targetIterator->state = BattleUnitState::Dead;
                targetIterator->targetId.reset();
                targetIterator->firstInRangeFrame.clear();
            }
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
            if (unit.state == BattleUnitState::Alive)
            {
                unit.attackElapsed += FixedDeltaSeconds;
                clearInvalidTarget(unit);
            }
        }

        for (BattleUnit& unit : units_)
        {
            if (unit.state == BattleUnitState::Alive
                && !unit.targetId.has_value())
            {
                moveUnit(unit);
            }
        }
        applyFrameGuardDamage();

        updateTargets();
        applyAttacks();

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
