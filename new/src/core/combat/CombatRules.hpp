#pragma once

#include "core/combat/BattleTypes.hpp"

namespace autochess::core
{
    class CombatRules
    {
    public:
        static double attackInterval(double attackSpeed) noexcept;

        static double physicalDamage(
            double attackPower,
            double physicalDefense) noexcept;

        static double magicDamage(
            double spellDamage,
            double magicResistance) noexcept;

        static double distance(
            BattlePosition left,
            BattlePosition right) noexcept;
    };
}
