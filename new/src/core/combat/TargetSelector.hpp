#pragma once

#include "core/combat/BattleTypes.hpp"

#include <cstdint>
#include <vector>

namespace autochess::core
{
    class TargetSelector
    {
    public:
        // 更新一个单位的入圈帧记录，并按帧号、编号选择当前目标。
        static void update(
            BattleUnit& attacker,
            const std::vector<BattleUnit>& units,
            std::uint64_t frame);
    };
}
