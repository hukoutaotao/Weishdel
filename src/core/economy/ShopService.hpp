#pragma once

#include "core/economy/EconomyTypes.hpp"
#include "core/model/Definitions.hpp"
#include "core/model/PlayerTypes.hpp"

#include <cstddef>
#include <random>
#include <vector>

namespace autochess::core
{
    class ShopService
    {
    public:
        static CommandResult rebuild(
            const PlayerState& player,
            ShopState& shop,
            const GameConfig& gameConfig,
            const std::vector<UnitDefinition>& units,
            const FactionDefinition& faction,
            const std::vector<FactionModifierDefinition>& modifiers,
            std::mt19937& randomEngine);

        static CommandResult refresh(
            PlayerState& player,
            ShopState& shop,
            const GameConfig& gameConfig,
            const std::vector<UnitDefinition>& units,
            const FactionDefinition& faction,
            const std::vector<FactionModifierDefinition>& modifiers,
            std::mt19937& randomEngine);

        static CommandResult purchase(
            PlayerState& player,
            ShopState& shop,
            std::size_t slot,
            const std::vector<UnitDefinition>& units,
            OwnedUnitId& nextOwnedUnitId);
    };
}
