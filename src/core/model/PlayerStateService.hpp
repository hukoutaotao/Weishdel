#pragma once

#include "core/model/Definitions.hpp"
#include "core/model/PlayerTypes.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace autochess::core
{
    class PlayerStateService
    {
    public:
        static PlayerState createInitial(
            MapSide side,
            const GameConfig& gameConfig,
            const FactionDefinition& faction);

        static const OwnedUnit* findActive(
            const PlayerState& player,
            OwnedUnitId unitId) noexcept;

        static const OwnedUnit* findDead(
            const PlayerState& player,
            OwnedUnitId unitId) noexcept;

        static std::optional<std::size_t> findReserveSlot(
            const PlayerState& player,
            OwnedUnitId unitId) noexcept;

        static std::optional<GridPosition> findDeploymentPosition(
            const PlayerState& player,
            OwnedUnitId unitId) noexcept;

        static std::optional<std::size_t> findFirstEmptyReserveSlot(
            const PlayerState& player) noexcept;

        static std::size_t activeCount(const PlayerState& player) noexcept;

        static std::size_t deployedCount(const PlayerState& player) noexcept;

        static bool validate(
            const PlayerState& player,
            std::string& errorMessage);
    };
}
