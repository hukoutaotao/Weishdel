#include "core/combat/CombatRules.hpp"

#include <algorithm>
#include <cmath>

namespace autochess::core
{
    double CombatRules::attackInterval(const double attackSpeed) noexcept
    {
        const double limitedSpeed = std::clamp(attackSpeed, 0.0, 600.0);
        return 200.0 / (100.0 + limitedSpeed);
    }

    double CombatRules::physicalDamage(
        const double attackPower,
        const double physicalDefense) noexcept
    {
        return std::max(attackPower - physicalDefense, 5.0);
    }

    double CombatRules::magicDamage(
        const double spellDamage,
        const double magicResistance) noexcept
    {
        const double limitedResistance =
            std::clamp(magicResistance, 0.0, 100.0);
        return std::max(
            0.0,
            spellDamage * (100.0 - limitedResistance) / 100.0);
    }

    double CombatRules::distance(
        const BattlePosition left,
        const BattlePosition right) noexcept
    {
        const double deltaX = left.x - right.x;
        const double deltaY = left.y - right.y;
        return std::sqrt(deltaX * deltaX + deltaY * deltaY);
    }
}
