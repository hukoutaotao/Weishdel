#pragma once

#include "core/model/Definitions.hpp"

#include <string>
#include <vector>

namespace autochess::core
{
    enum class PriceErrorCode
    {
        None,
        InvalidBasePrice,
        InvalidFactionMultiplier,
        InvalidPriceModifier,
        InvalidAdjustedPrice,
        InvalidRatio
    };

    struct PriceCalculationResult
    {
        bool success = false;
        int amount = 0;
        PriceErrorCode errorCode = PriceErrorCode::None;
        std::string message;
    };

    class PriceRules
    {
    public:
        static PriceCalculationResult calculateLevelOnePrice(
            const UnitDefinition& unit,
            const FactionDefinition& faction,
            const std::vector<FactionModifierDefinition>& modifiers);

        static PriceCalculationResult calculateRatioAmount(
            int levelOnePrice,
            double ratio);
    };
}
