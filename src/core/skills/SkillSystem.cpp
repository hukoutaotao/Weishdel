#include "core/skills/SkillSystem.hpp"

#include "core/combat/CombatRules.hpp"

#include <algorithm>
#include <cstddef>

namespace autochess::core
{
    namespace
    {
        // 此结构保存敌方技能候选的距离和编号排序信息。
        struct EnemyCandidate
        {
            BattleUnitId id = InvalidBattleUnitId;
            double distance = 0.0;
        };

        // 此结构保存友方治疗候选的生命比例和编号排序信息。
        struct AllyCandidate
        {
            BattleUnitId id = InvalidBattleUnitId;
            double healthRatio = 0.0;
        };

        // 此函数把排好序的候选编号限制到技能配置的目标数量。
        template<typename Candidate>
        std::vector<BattleUnitId> takeCandidateIds(
            const std::vector<Candidate>& candidates,
            const int targetCount)
        {
            const std::size_t resultCount = std::min(
                candidates.size(),
                static_cast<std::size_t>(targetCount));
            std::vector<BattleUnitId> result;
            result.reserve(resultCount);
            for (std::size_t index = 0; index < resultCount; ++index)
            {
                result.push_back(candidates[index].id);
            }
            return result;
        }

        // 此函数按距离优先、编号次优先选择范围内的存活敌人。
        std::vector<BattleUnitId> selectEnemies(
            const BattleUnit& caster,
            const SkillDefinition& skill,
            const std::vector<BattleUnit>& units)
        {
            std::vector<EnemyCandidate> candidates;
            for (const BattleUnit& candidate : units)
            {
                const double distance = CombatRules::distance(
                    caster.position,
                    candidate.position);
                if (candidate.id == InvalidBattleUnitId
                    || candidate.state != BattleUnitState::Alive
                    || candidate.side == caster.side
                    || distance > skill.effectRange)
                {
                    continue;
                }
                candidates.push_back(EnemyCandidate{candidate.id, distance});
            }

            // 此代码块保证多目标技能不受单位容器遍历顺序影响。
            std::sort(
                candidates.begin(),
                candidates.end(),
                [](const EnemyCandidate& left, const EnemyCandidate& right)
                {
                    if (left.distance != right.distance)
                    {
                        return left.distance < right.distance;
                    }
                    return left.id < right.id;
                });
            return takeCandidateIds(candidates, skill.targetCount);
        }

        // 此函数按最低生命比例优先、编号次优先选择范围内受伤友军。
        std::vector<BattleUnitId> selectLowestHealthAllies(
            const BattleUnit& caster,
            const SkillDefinition& skill,
            const std::vector<BattleUnit>& units)
        {
            std::vector<AllyCandidate> candidates;
            for (const BattleUnit& candidate : units)
            {
                if (candidate.id == InvalidBattleUnitId
                    || candidate.state != BattleUnitState::Alive
                    || candidate.side != caster.side
                    || (!skill.allowSelf && candidate.id == caster.id)
                    || candidate.stats.maxHealth <= 0.0
                    || candidate.health >= candidate.stats.maxHealth
                    || CombatRules::distance(
                           caster.position,
                           candidate.position)
                        > skill.effectRange)
                {
                    continue;
                }
                candidates.push_back(AllyCandidate{
                    candidate.id,
                    candidate.health / candidate.stats.maxHealth});
            }

            // 此代码块保证治疗技能在生命比例相同时稳定选择较小编号。
            std::sort(
                candidates.begin(),
                candidates.end(),
                [](const AllyCandidate& left, const AllyCandidate& right)
                {
                    if (left.healthRatio != right.healthRatio)
                    {
                        return left.healthRatio < right.healthRatio;
                    }
                    return left.id < right.id;
                });
            return takeCandidateIds(candidates, skill.targetCount);
        }
    }

    void SkillSystem::regenerateMana(
        BattleUnit& unit,
        const double deltaSeconds) noexcept
    {
        if (unit.state != BattleUnitState::Alive
            || unit.activeSkill.active
            || unit.maxMana <= 0.0
            || deltaSeconds <= 0.0)
        {
            return;
        }

        // 此代码块按每秒一点恢复技力并把结果限制在零到最大值之间。
        unit.currentMana = std::clamp(
            unit.currentMana + deltaSeconds,
            0.0,
            unit.maxMana);
    }

    bool SkillSystem::canRelease(const BattleUnit& unit) noexcept
    {
        return unit.state == BattleUnitState::Alive
            && !unit.activeSkill.active
            && unit.maxMana > 0.0
            && unit.currentMana >= unit.maxMana;
    }

    std::vector<BattleUnitId> SkillSystem::selectTargets(
        const BattleUnit& caster,
        const SkillDefinition& skill,
        const std::vector<BattleUnit>& units)
    {
        if (caster.state != BattleUnitState::Alive
            || caster.id == InvalidBattleUnitId
            || skill.targetCount <= 0
            || skill.effectRange < 0.0)
        {
            return {};
        }

        // 此代码块把三种技能目标规则分派到各自的确定性选择逻辑。
        if (skill.targetRule == SkillTargetRule::Self)
        {
            return skill.allowSelf
                ? std::vector<BattleUnitId>{caster.id}
                : std::vector<BattleUnitId>{};
        }
        if (skill.targetRule == SkillTargetRule::Enemy)
        {
            return selectEnemies(caster, skill, units);
        }
        if (skill.targetRule == SkillTargetRule::LowestHealthAlly)
        {
            return selectLowestHealthAllies(caster, skill, units);
        }
        return {};
    }
}
