#pragma once

#include "core/economy/EconomyTypes.hpp"
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
    };
}
