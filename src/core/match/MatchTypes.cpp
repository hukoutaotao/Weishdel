#include "core/match/MatchTypes.hpp"

#include <ostream>

// 此命名空间实现对局摘要的控制台文本格式。
namespace autochess::core
{
    // 此辅助函数把阵营枚举转换为便于阅读的固定文本。
    const char* sideText(const MapSide side) noexcept
    {
        // 此分支为每种阵营提供确定且不随语言环境变化的输出。
        switch (side)
        {
        case MapSide::A:
            return "A";
        case MapSide::B:
            return "B";
        default:
            return "none";
        }
    }

    // 此辅助函数把最终胜负枚举转换为便于阅读的固定文本。
    const char* outcomeText(const MatchOutcome outcome) noexcept
    {
        // 此分支为每种对局结果提供确定且可测试的输出。
        switch (outcome)
        {
        case MatchOutcome::SideAWin:
            return "A_win";
        case MatchOutcome::SideBWin:
            return "B_win";
        case MatchOutcome::Draw:
            return "draw";
        default:
            return "ongoing";
        }
    }

    // 此输出运算符按回合号、伤害、守卫和败者顺序生成日志。
    std::ostream& operator<<(
        std::ostream& output,
        const RoundSummary& summary)
    {
        output
            << "[ROUND " << summary.roundNumber << "]"
            << " damage_to_A=" << summary.battle.guardDamageToA
            << " damage_to_B=" << summary.battle.guardDamageToB
            << " guard_A=" << summary.guardValueA
            << " guard_B=" << summary.guardValueB
            << " loser=" << sideText(summary.loserSide);
        return output;
    }

    // 此输出运算符按结果、回合数和双方守卫值生成最终日志。
    std::ostream& operator<<(
        std::ostream& output,
        const MatchResultSummary& summary)
    {
        output
            << "[MATCH] outcome=" << outcomeText(summary.outcome)
            << " rounds=" << summary.completedRounds
            << " guard_A=" << summary.guardValueA
            << " guard_B=" << summary.guardValueB;
        return output;
    }
}
