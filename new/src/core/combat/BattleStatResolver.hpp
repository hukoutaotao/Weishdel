#pragma once

#include "core/combat/BattleTypes.hpp"
#include "core/model/Definitions.hpp"

#include <string>
#include <vector>

namespace autochess::core
{
    // 此服务从不可变单位定义计算一次分队修正后的战斗基础属性。
    class BattleStatResolver
    {
    public:
        static bool resolve(
            const UnitDefinition& unit,
            int level,
            const FactionDefinition& faction,
            const std::vector<FactionModifierDefinition>& modifiers,
            BattleStats& output,
            std::string& errorMessage);
    };
}
