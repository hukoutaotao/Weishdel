#pragma once

#include <string>

namespace spine
{
    class SkeletonData;
}

namespace autochess::game
{
    // 此结构把素材中的原始动作名归一化为游戏需要的七类动作。
    struct UnitAnimationManifest
    {
        std::string relax;
        std::string move;
        std::string start;
        std::string attack;
        std::string attackBegin;
        std::string attackEnd;
        std::string die;

        // 此函数扫描一个 Spine skeleton 的动作列表并选择稳定的动作名称。
        static UnitAnimationManifest fromSkeletonData(
            spine::SkeletonData& skeletonData);

        bool hasRelax() const noexcept { return !relax.empty(); }
        bool hasMove() const noexcept { return !move.empty(); }
        bool hasStart() const noexcept { return !start.empty(); }
        bool hasAttack() const noexcept { return !attack.empty(); }
        bool hasAttackSequence() const noexcept
        {
            return !attackBegin.empty() && !attack.empty() && !attackEnd.empty();
        }
        bool hasDie() const noexcept { return !die.empty(); }
    };
}