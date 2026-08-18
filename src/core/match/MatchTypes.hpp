#pragma once

#include "core/combat/BattleTypes.hpp"
#include "core/map/MapTypes.hpp"

#include <iosfwd>

// 此命名空间集中保存对局编排层使用的公共类型。
namespace autochess::core
{
    // 此枚举描述从选择界面到最终结果的完整对局阶段。
    enum class MatchPhase
    {
        MapSelection,
        FactionSelection,
        AiSelection,
        Preparation,
        Combat,
        RoundSettlement,
        MatchResult
    };

    // 此枚举保存玩家为电脑选择的三种正式策略类型。
    enum class AiStrategyKind
    {
        Unknown,
        Offensive,
        Defensive,
        Route
    };

    // 此枚举描述尚未结束或已经确定的最终对局结果。
    enum class MatchOutcome
    {
        Ongoing,
        SideAWin,
        SideBWin,
        Draw
    };

    // 此结构保存一个已经完成回合的战斗和结算摘要。
    struct RoundSummary
    {
        int roundNumber = 0;
        BattleSummary battle;
        int guardValueA = 0;
        int guardValueB = 0;
        MapSide loserSide = MapSide::Unknown;
    };

    // 此结构保存对局结束时的胜负和实际完成回合数。
    struct MatchResultSummary
    {
        MatchOutcome outcome = MatchOutcome::Ongoing;
        int completedRounds = 0;
        int guardValueA = 0;
        int guardValueB = 0;
    };

    // 此输出运算符把回合摘要写成稳定的控制台日志。
    std::ostream& operator<<(std::ostream& output, const RoundSummary& summary);

    // 此输出运算符把最终对局结果写成稳定的控制台日志。
    std::ostream& operator<<(
        std::ostream& output,
        const MatchResultSummary& summary);
}
