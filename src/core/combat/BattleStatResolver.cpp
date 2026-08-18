#include "core/combat/BattleStatResolver.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace autochess::core
{
    namespace
    {
        // 此结构按属性独立汇总全部加法和乘法，避免配置顺序改变结果。
        struct ModifierTotals
        {
            double additive = 0.0;
            double multiplicative = 1.0;
        };

        constexpr std::size_t attributeIndex(
            const FactionAttribute attribute) noexcept
        {
            return static_cast<std::size_t>(attribute);
        }

        bool fail(std::string& errorMessage, const char* message)
        {
            errorMessage = message;
            return false;
        }

        // 此函数把指定属性的汇总修正应用到一个基础数值。
        double adjusted(
            const double baseValue,
            const FactionAttribute attribute,
            const std::array<ModifierTotals, 10>& totals) noexcept
        {
            const ModifierTotals& total = totals[attributeIndex(attribute)];
            return (baseValue + total.additive) * total.multiplicative;
        }
    }

    bool BattleStatResolver::resolve(
        const UnitDefinition& unit,
        const int level,
        const FactionDefinition& faction,
        const std::vector<FactionModifierDefinition>& modifiers,
        BattleStats& output,
        std::string& errorMessage)
    {
        errorMessage.clear();
        if (level < 1 || level > 3)
        {
            return fail(errorMessage, "战斗单位等级必须在 1 到 3 之间");
        }
        if (unit.id.empty() || faction.id.empty())
        {
            return fail(errorMessage, "战斗属性解析要求有效的单位和分队 ID");
        }

        // 此代码块校验等级倍率并汇总当前分队、当前单位的属性修正。
        const double levelMultiplier =
            unit.levelMultipliers[static_cast<std::size_t>(level - 1)];
        if (!std::isfinite(levelMultiplier) || levelMultiplier <= 0.0)
        {
            return fail(errorMessage, "单位等级倍率必须是正数");
        }

        std::array<ModifierTotals, 10> totals{};
        for (const FactionModifierDefinition& modifier : modifiers)
        {
            if (modifier.factionId != faction.id
                || (modifier.unitId != unit.id && modifier.unitId != "*")
                || modifier.attribute == FactionAttribute::Price)
            {
                continue;
            }
            if (modifier.attribute == FactionAttribute::Unknown
                || !std::isfinite(modifier.value))
            {
                return fail(errorMessage, "分队战斗属性修正无效");
            }

            ModifierTotals& total = totals[attributeIndex(modifier.attribute)];
            if (modifier.operation == FactionOperation::Add)
            {
                total.additive += modifier.value;
            }
            else if (modifier.operation == FactionOperation::Multiply
                && modifier.value > 0.0)
            {
                total.multiplicative *= modifier.value;
            }
            else
            {
                return fail(errorMessage, "分队战斗属性修正运算无效");
            }
        }

        // 此代码块先计算等级属性，再一次性应用分队修正和规则边界。
        BattleStats resolved;
        resolved.maxHealth = adjusted(
            static_cast<double>(unit.maxHealth) * levelMultiplier,
            FactionAttribute::MaxHealth,
            totals);
        resolved.attackPower = adjusted(
            static_cast<double>(unit.attackPower) * levelMultiplier,
            FactionAttribute::AttackPower,
            totals);
        resolved.physicalDefense = adjusted(
            static_cast<double>(unit.physicalDefense) * levelMultiplier,
            FactionAttribute::PhysicalDefense,
            totals);
        resolved.magicResistance = std::clamp(
            adjusted(
                static_cast<double>(unit.magicResistance) * levelMultiplier,
                FactionAttribute::MagicResistance,
                totals),
            0.0,
            100.0);
        resolved.attackRange = adjusted(
            unit.attackRange * levelMultiplier,
            FactionAttribute::AttackRange,
            totals);
        resolved.moveSpeed = adjusted(
            unit.moveSpeed * levelMultiplier,
            FactionAttribute::MoveSpeed,
            totals);
        resolved.attackSpeed = std::clamp(
            adjusted(
                static_cast<double>(unit.attackSpeed) * levelMultiplier,
                FactionAttribute::AttackSpeed,
                totals),
            0.0,
            600.0);
        resolved.guardDamage = static_cast<int>(std::round(adjusted(
            static_cast<double>(unit.guardDamage) * levelMultiplier,
            FactionAttribute::GuardDamage,
            totals)));

        // 此代码块拒绝修正后会破坏战斗不变量的属性集合。
        if (!std::isfinite(resolved.maxHealth)
            || !std::isfinite(resolved.attackPower)
            || !std::isfinite(resolved.physicalDefense)
            || !std::isfinite(resolved.attackRange)
            || !std::isfinite(resolved.moveSpeed)
            || resolved.maxHealth <= 0.0
            || resolved.attackPower < 0.0
            || resolved.physicalDefense < 0.0
            || resolved.attackRange <= 0.0
            || resolved.moveSpeed <= 0.0
            || resolved.guardDamage <= 0)
        {
            return fail(errorMessage, "分队修正后的战斗属性超出合法范围");
        }

        output = resolved;
        return true;
    }
}
