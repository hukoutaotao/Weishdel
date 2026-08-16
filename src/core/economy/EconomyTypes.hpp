#pragma once

#include <optional>
#include <string>
#include <vector>

namespace autochess::core
{
    struct ShopOffer
    {
        std::string unitId;
        int displayedPrice = 0;
    };

    struct ShopState
    {
        std::vector<std::optional<ShopOffer>> offers;
    };

    enum class CommandErrorCode
    {
        None,
        InvalidConfiguration,
        InvalidShopSlot,
        EmptyShopSlot,
        InsufficientGold,
        RosterFull,
        UnitNotFound,
        WrongOwner,
        InvalidTarget,
        TargetOccupied,
        DeploymentLimitReached,
        NotMergeable,
        MaxLevelReached,
        UnitNotDead,
        UnitAlreadyActive,
        UnitAlreadyDead,
        InconsistentState
    };

    struct CommandResult
    {
        bool success = false;
        CommandErrorCode errorCode = CommandErrorCode::None;
        std::string message;
    };
}
