#include "core/combat/TargetSelector.hpp"

#include "core/combat/CombatRules.hpp"

#include <algorithm>
#include <map>

namespace autochess::core
{
    namespace
    {
        const BattleUnit* findUnit(
            const std::vector<BattleUnit>& units,
            const BattleUnitId id)
        {
            const auto iterator = std::find_if(
                units.begin(),
                units.end(),
                [id](const BattleUnit& unit)
                {
                    return unit.id == id;
                });
            return iterator == units.end() ? nullptr : &*iterator;
        }

        bool isCandidate(
            const BattleUnit& attacker,
            const BattleUnit& target)
        {
            return target.state == BattleUnitState::Alive
                && target.side != attacker.side
                && CombatRules::distance(
                       attacker.position,
                       target.position)
                    <= attacker.stats.attackRange;
        }
    }

    void TargetSelector::update(
        BattleUnit& attacker,
        const std::vector<BattleUnit>& units,
        const std::uint64_t frame)
    {
        if (attacker.state != BattleUnitState::Alive)
        {
            attacker.targetId.reset();
            attacker.firstInRangeFrame.clear();
            return;
        }

        std::map<BattleUnitId, std::uint64_t> currentCandidates;
        for (const BattleUnit& target : units)
        {
            if (!isCandidate(attacker, target))
            {
                continue;
            }

            const auto knownFrame = attacker.firstInRangeFrame.find(target.id);
            const std::uint64_t firstFrame = knownFrame ==
                    attacker.firstInRangeFrame.end()
                ? frame
                : knownFrame->second;
            currentCandidates.emplace(target.id, firstFrame);
        }

        attacker.firstInRangeFrame = currentCandidates;
        if (currentCandidates.empty())
        {
            attacker.targetId.reset();
            return;
        }

        const auto selected = std::min_element(
            currentCandidates.begin(),
            currentCandidates.end(),
            [](const auto& left, const auto& right)
            {
                if (left.second != right.second)
                {
                    return left.second < right.second;
                }

                return left.first < right.first;
            });
        attacker.targetId = selected->first;

        if (findUnit(units, selected->first) == nullptr)
        {
            attacker.targetId.reset();
        }
    }
}
