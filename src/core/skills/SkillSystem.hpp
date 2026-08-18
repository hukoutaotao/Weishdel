#pragma once

#include "core/combat/BattleTypes.hpp"
#include "core/model/Definitions.hpp"

#include <vector>

namespace autochess::core
{
    // 此服务集中处理技力恢复、释放条件和配置驱动的确定性技能选目标规则。
    class SkillSystem
    {
    public:
        static void regenerateMana(
            BattleUnit& unit,
            double deltaSeconds) noexcept;

        static bool canRelease(const BattleUnit& unit) noexcept;

        static std::vector<BattleUnitId> selectTargets(
            const BattleUnit& caster,
            const SkillDefinition& skill,
            const std::vector<BattleUnit>& units);
    };
}
