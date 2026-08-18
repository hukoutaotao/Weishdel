#include "core/skills/SkillSystem.hpp"

#include "core/combat/CombatRules.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

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

        // 此函数按战斗单位编号查找可修改的临时战斗实例。
        BattleUnit* findUnit(
            std::vector<BattleUnit>& units,
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

        // 此函数安全读取施法者等级对应的技能效果数值。
        bool readSkillValue(
            const BattleUnit& caster,
            const SkillDefinition& skill,
            double& value) noexcept
        {
            if (caster.identity.level < 1 || caster.identity.level > 3)
            {
                return false;
            }
            value = skill.levelValues[
                static_cast<std::size_t>(caster.identity.level - 1)];
            return std::isfinite(value) && value >= 0.0;
        }

        // 此函数把增益运算应用到分队解析后的基础属性而不累积旧技能结果。
        bool applyModifier(
            const double baseValue,
            const ModifierMode mode,
            const double modifierValue,
            double& output) noexcept
        {
            if (mode == ModifierMode::Add)
            {
                output = baseValue + modifierValue;
                return true;
            }
            if (mode == ModifierMode::Multiply)
            {
                output = baseValue * modifierValue;
                return true;
            }
            return false;
        }

        // 此函数修改配置指定的实时属性并保留其他属性不变。
        bool applyBuffStat(
            BattleUnit& caster,
            const SkillDefinition& skill,
            const double value) noexcept
        {
            if (skill.buffStat == BuffStat::AttackPower)
            {
                return applyModifier(
                    caster.baseStats.attackPower,
                    skill.modifierMode,
                    value,
                    caster.stats.attackPower);
            }
            if (skill.buffStat == BuffStat::PhysicalDefense)
            {
                return applyModifier(
                    caster.baseStats.physicalDefense,
                    skill.modifierMode,
                    value,
                    caster.stats.physicalDefense);
            }
            if (skill.buffStat == BuffStat::MagicResistance)
            {
                return applyModifier(
                    caster.baseStats.magicResistance,
                    skill.modifierMode,
                    value,
                    caster.stats.magicResistance);
            }
            if (skill.buffStat == BuffStat::MoveSpeed)
            {
                return applyModifier(
                    caster.baseStats.moveSpeed,
                    skill.modifierMode,
                    value,
                    caster.stats.moveSpeed);
            }
            if (skill.buffStat == BuffStat::AttackSpeed)
            {
                return applyModifier(
                    caster.baseStats.attackSpeed,
                    skill.modifierMode,
                    value,
                    caster.stats.attackSpeed);
            }
            return false;
        }

        // 此函数把技能修改过的属性恢复为分队与等级解析后的基础值。
        void restoreBuffStat(BattleUnit& unit) noexcept
        {
            if (unit.activeSkill.buffStat == BuffStat::AttackPower)
            {
                unit.stats.attackPower = unit.baseStats.attackPower;
            }
            else if (unit.activeSkill.buffStat == BuffStat::PhysicalDefense)
            {
                unit.stats.physicalDefense = unit.baseStats.physicalDefense;
            }
            else if (unit.activeSkill.buffStat == BuffStat::MagicResistance)
            {
                unit.stats.magicResistance = unit.baseStats.magicResistance;
            }
            else if (unit.activeSkill.buffStat == BuffStat::MoveSpeed)
            {
                unit.stats.moveSpeed = unit.baseStats.moveSpeed;
            }
            else if (unit.activeSkill.buffStat == BuffStat::AttackSpeed)
            {
                unit.stats.attackSpeed = unit.baseStats.attackSpeed;
            }
            unit.activeSkill = ActiveSkillState{};
        }

        // 此函数把技能持续秒数转换为至少一帧的六十帧固定步长计数。
        bool durationFrames(
            const double durationSeconds,
            std::uint64_t& frames) noexcept
        {
            constexpr double framesPerSecond = 60.0;
            const double rawFrames = std::ceil(
                durationSeconds * framesPerSecond - 1.0e-9);
            if (!std::isfinite(rawFrames)
                || rawFrames < 1.0
                || rawFrames
                    > static_cast<double>(
                        std::numeric_limits<std::uint64_t>::max()))
            {
                return false;
            }
            frames = static_cast<std::uint64_t>(rawFrames);
            return true;
        }

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

    bool SkillSystem::release(
        const BattleUnitId casterId,
        const SkillDefinition& skill,
        std::vector<BattleUnit>& units)
    {
        BattleUnit* caster = findUnit(units, casterId);
        if (caster == nullptr
            || skill.id.empty()
            || caster->skillId != skill.id
            || !canRelease(*caster))
        {
            return false;
        }

        // 此代码块在修改战斗状态前解析等级数值和全部合法目标以保证失败不消耗技力。
        double effectValue = 0.0;
        if (!readSkillValue(*caster, skill, effectValue))
        {
            return false;
        }
        const std::vector<BattleUnitId> targetIds = selectTargets(
            *caster,
            skill,
            units);
        if (targetIds.empty())
        {
            return false;
        }
        for (const BattleUnitId targetId : targetIds)
        {
            if (findUnit(units, targetId) == nullptr)
            {
                return false;
            }
        }

        // 此代码块执行物理或法术技能伤害并立即标记生命归零的目标。
        if (skill.effectType == SkillEffectType::Damage)
        {
            if (skill.damageType != DamageType::Physical
                && skill.damageType != DamageType::Magic)
            {
                return false;
            }
            for (const BattleUnitId targetId : targetIds)
            {
                BattleUnit* target = findUnit(units, targetId);
                const double damage = skill.damageType == DamageType::Physical
                    ? CombatRules::physicalDamage(
                        effectValue,
                        target->stats.physicalDefense)
                    : CombatRules::magicDamage(
                        effectValue,
                        target->stats.magicResistance);
                target->health -= damage;
                if (target->health <= 0.0)
                {
                    target->health = 0.0;
                    target->state = BattleUnitState::Dead;
                    target->targetId.reset();
                    target->firstInRangeFrame.clear();
                    restoreBuffStat(*target);
                }
            }
        }
        // 此代码块执行治疗技能并把每个目标生命值限制在最大生命值以内。
        else if (skill.effectType == SkillEffectType::Heal)
        {
            for (const BattleUnitId targetId : targetIds)
            {
                BattleUnit* target = findUnit(units, targetId);
                target->health = std::min(
                    target->stats.maxHealth,
                    target->health + effectValue);
            }
        }
        // 此代码块启动自身限时增益并记录后续帧需要遵守的行为许可。
        else if (skill.effectType == SkillEffectType::Buff)
        {
            std::uint64_t frames = 0;
            if (targetIds.size() != 1
                || targetIds.front() != caster->id
                || !durationFrames(skill.durationSeconds, frames)
                || !applyBuffStat(*caster, skill, effectValue))
            {
                return false;
            }
            caster->activeSkill.active = true;
            caster->activeSkill.remainingFrames = frames;
            caster->activeSkill.allowMove = skill.allowMove;
            caster->activeSkill.allowBasicAction = skill.allowBasicAction;
            caster->activeSkill.buffStat = skill.buffStat;
            caster->activeSkill.modifierMode = skill.modifierMode;
            caster->activeSkill.modifierValue = effectValue;
        }
        else
        {
            return false;
        }

        // 此代码块只在技能效果成功生效后一次性清空施法者技力。
        caster->currentMana = 0.0;
        return true;
    }

    void SkillSystem::advanceActiveSkill(BattleUnit& unit) noexcept
    {
        if (!unit.activeSkill.active)
        {
            return;
        }

        // 此代码块在死亡或最后一帧结束时恢复基础属性并清除技能状态。
        if (unit.state != BattleUnitState::Alive
            || unit.activeSkill.remainingFrames <= 1)
        {
            restoreBuffStat(unit);
            return;
        }
        --unit.activeSkill.remainingFrames;
    }
}
