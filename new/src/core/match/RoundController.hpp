#pragma once

#include "core/economy/EconomyTypes.hpp"
#include "core/match/MatchTypes.hpp"
#include "core/model/Definitions.hpp"
#include "core/model/PlayerTypes.hpp"

#include <random>
#include <vector>

// 此命名空间声明不依赖界面的回合准备和结算服务。
namespace autochess::core
{
    // 此服务集中执行跨回合状态变更并保证失败时不留下半更新状态。
    class RoundController
    {
    public:
        // 此函数发放本回合收入和败者补助后重新生成双方商店。
        static CommandResult beginPreparation(
            PlayerState& playerA,
            PlayerState& playerB,
            ShopState& shopA,
            ShopState& shopB,
            const GameConfig& gameConfig,
            const std::vector<UnitDefinition>& units,
            const std::vector<FactionDefinition>& factions,
            const std::vector<FactionModifierDefinition>& modifiers,
            MapSide previousLoser,
            std::mt19937& randomEngine);

        // 此函数把战斗摘要原子地回写到双方持久状态并生成回合摘要。
        static CommandResult settleBattle(
            PlayerState& playerA,
            PlayerState& playerB,
            const BattleSummary& battleSummary,
            int roundNumber,
            RoundSummary& roundSummary,
            bool guardDamageAlreadyApplied = false);

        // 此函数按照守卫归零和最大回合规则计算当前最终结果。
        static MatchResultSummary determineMatchResult(
            const PlayerState& playerA,
            const PlayerState& playerB,
            const GameConfig& gameConfig,
            int completedRounds) noexcept;
    };
}
