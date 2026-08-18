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

        // 此函数判断敌方单位能否成为普通攻击目标。
        bool isAttackCandidate(
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

        // 此函数判断未满血友军能否成为普通治疗目标。
        bool isHealCandidate(
            const BattleUnit& healer,
            const BattleUnit& target)
        {
            return target.state == BattleUnitState::Alive
                && target.side == healer.side
                && target.health < target.stats.maxHealth
                && CombatRules::distance(
                       healer.position,
                       target.position)
                    <= healer.stats.attackRange;
        }

        // 此函数按生命比例和编号选择确定性的普通治疗目标。
        void updateHealTarget(
            BattleUnit& healer,
            const std::vector<BattleUnit>& units)
        {
            healer.firstInRangeFrame.clear();
            const BattleUnit* selected = nullptr;
            for (const BattleUnit& candidate : units)
            {
                if (!isHealCandidate(healer, candidate))
                {
                    continue;
                }

                const double candidateRatio =
                    candidate.health / candidate.stats.maxHealth;
                const double selectedRatio = selected == nullptr
                    ? 0.0
                    : selected->health / selected->stats.maxHealth;
                if (selected == nullptr
                    || candidateRatio < selectedRatio
                    || (candidateRatio == selectedRatio
                        && candidate.id < selected->id))
                {
                    selected = &candidate;
                }
            }

            healer.targetId = selected == nullptr
                ? std::optional<BattleUnitId>{}
                : std::optional<BattleUnitId>{selected->id};
        }

        // 此函数保留第 4 天首次入圈帧和编号并列的攻击选敌规则。
        void updateAttackTarget(
            BattleUnit& attacker,
            const std::vector<BattleUnit>& units,
            const std::uint64_t frame)
        {
            std::map<BattleUnitId, std::uint64_t> currentCandidates;
            for (const BattleUnit& target : units)
            {
                if (!isAttackCandidate(attacker, target))
                {
                    continue;
                }

                const auto knownFrame =
                    attacker.firstInRangeFrame.find(target.id);
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

        // 此代码块为攻击和治疗普通行动选择各自规则对应的目标。
        if (attacker.basicAction == BasicAction::Attack)
        {
            updateAttackTarget(attacker, units, frame);
            return;
        }
        if (attacker.basicAction == BasicAction::Heal)
        {
            updateHealTarget(attacker, units);
            return;
        }

        attacker.targetId.reset();
        attacker.firstInRangeFrame.clear();
    }
}
