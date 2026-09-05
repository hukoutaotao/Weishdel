#include "core/economy/PriceRules.hpp"

#include <cmath>
#include <limits>

namespace autochess::core
{
    namespace
    {
        PriceCalculationResult fail(
            const PriceErrorCode errorCode,
            const char* message)
        {
            PriceCalculationResult result;
            result.errorCode = errorCode;
            result.message = message;
            return result;
        }

        PriceCalculationResult success(const double roundedAmount)
        {
            PriceCalculationResult result;
            result.success = true;
            result.amount = static_cast<int>(roundedAmount);
            result.errorCode = PriceErrorCode::None;
            return result;
        }

        bool isRepresentableAsInt(const double value)
        {
            return std::isfinite(value)
                && value <= static_cast<double>(
                    std::numeric_limits<int>::max());
        }
    }

    PriceCalculationResult PriceRules::calculateLevelOnePrice(
        const UnitDefinition& unit,
        const FactionDefinition& faction,
        const std::vector<FactionModifierDefinition>& modifiers)
    {
        if (unit.id.empty() || unit.price < 1)
        {
            return fail(
                PriceErrorCode::InvalidBasePrice,
                "单位一级基础价格必须为正数");
        }

        if (faction.id.empty()
            || !std::isfinite(faction.priceMultiplier)
            || faction.priceMultiplier <= 0.0)
        {
            return fail(
                PriceErrorCode::InvalidFactionMultiplier,
                "分队价格倍率必须为正数");
        }

        double additiveModifier = 0.0;
        double multiplicativeModifier = 1.0;
        for (const FactionModifierDefinition& modifier : modifiers)
        {
            if (modifier.factionId != faction.id
                || modifier.unitId != unit.id
                || modifier.attribute != FactionAttribute::Price)
            {
                continue;
            }

            if (!std::isfinite(modifier.value))
            {
                return fail(
                    PriceErrorCode::InvalidPriceModifier,
                    "价格修正值必须是有限数字");
            }

            if (modifier.operation == FactionOperation::Add)
            {
                additiveModifier += modifier.value;
            }
            else if (modifier.operation == FactionOperation::Multiply
                && modifier.value > 0.0)
            {
                multiplicativeModifier *= modifier.value;
            }
            else
            {
                return fail(
                    PriceErrorCode::InvalidPriceModifier,
                    "价格修正运算或倍率无效");
            }
        }

        const double adjustedPrice =
            (static_cast<double>(unit.price) + additiveModifier)
            * multiplicativeModifier
            * faction.priceMultiplier;
        if (!isRepresentableAsInt(adjustedPrice)
            || adjustedPrice < 1.0)
        {
            return fail(
                PriceErrorCode::InvalidAdjustedPrice,
                "修正后一级购买价必须为正数");
        }

        const double roundedPrice = std::round(adjustedPrice);
        if (!isRepresentableAsInt(roundedPrice)
            || roundedPrice < 1.0)
        {
            return fail(
                PriceErrorCode::InvalidAdjustedPrice,
                "四舍五入后的一级购买价必须为正数");
        }

        return success(roundedPrice);
    }

    PriceCalculationResult PriceRules::calculateRatioAmount(
        const int levelOnePrice,
        const double ratio)
    {
        if (levelOnePrice < 1
            || !std::isfinite(ratio)
            || ratio < 0.0
            || ratio > 1.0)
        {
            return fail(
                PriceErrorCode::InvalidRatio,
                "金额比例必须在 0 到 1 之间且一级购买价必须为正数");
        }

        const double amount =
            static_cast<double>(levelOnePrice) * ratio;
        if (!isRepresentableAsInt(amount))
        {
            return fail(
                PriceErrorCode::InvalidRatio,
                "比例金额超出整数范围");
        }

        return success(std::round(amount));
    }
}
