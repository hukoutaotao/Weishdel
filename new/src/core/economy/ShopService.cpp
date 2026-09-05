#include "core/economy/ShopService.hpp"

#include "core/economy/PriceRules.hpp"
#include "core/model/PlayerStateService.hpp"

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace autochess::core
{
    namespace
    {
        CommandResult fail(
            const CommandErrorCode errorCode,
            const std::string& message)
        {
            CommandResult result;
            result.errorCode = errorCode;
            result.message = message;
            return result;
        }

        CommandResult success(const char* message)
        {
            CommandResult result;
            result.success = true;
            result.errorCode = CommandErrorCode::None;
            result.message = message;
            return result;
        }

        bool containsOwnedUnitId(
            const PlayerState& player,
            const OwnedUnitId unitId)
        {
            return PlayerStateService::findActive(player, unitId) != nullptr
                || PlayerStateService::findDead(player, unitId) != nullptr;
        }

        CommandResult generateOffers(
            const PlayerState& player,
            ShopState& generatedShop,
            const GameConfig& gameConfig,
            const std::vector<UnitDefinition>& units,
            const FactionDefinition& faction,
            const std::vector<FactionModifierDefinition>& modifiers,
            std::mt19937& randomEngine)
        {
            if (gameConfig.shopSlots <= 0)
            {
                return fail(
                    CommandErrorCode::InvalidConfiguration,
                    "商店槽位数量必须为正数");
            }

            if (units.empty())
            {
                return fail(
                    CommandErrorCode::InvalidConfiguration,
                    "商店单位池不能为空");
            }

            if (player.factionId.empty()
                || faction.id.empty()
                || player.factionId != faction.id)
            {
                return fail(
                    CommandErrorCode::InconsistentState,
                    "玩家分队与商店价格分队不一致");
            }

            ShopState temporaryShop;
            temporaryShop.offers.reserve(
                static_cast<std::size_t>(gameConfig.shopSlots));
            std::uniform_int_distribution<std::size_t> distribution(
                0, units.size() - 1);

            for (int slot = 0; slot < gameConfig.shopSlots; ++slot)
            {
                const UnitDefinition& unit = units[distribution(randomEngine)];
                const PriceCalculationResult price =
                    PriceRules::calculateLevelOnePrice(
                        unit, faction, modifiers);
                if (!price.success)
                {
                    return fail(
                        CommandErrorCode::InvalidConfiguration,
                        "无法生成商店商品：" + price.message);
                }

                temporaryShop.offers.emplace_back(
                    ShopOffer{unit.id, price.amount});
            }

            generatedShop = std::move(temporaryShop);
            return success("商品生成成功");
        }
    }

    CommandResult ShopService::rebuild(
        const PlayerState& player,
        ShopState& shop,
        const GameConfig& gameConfig,
        const std::vector<UnitDefinition>& units,
        const FactionDefinition& faction,
        const std::vector<FactionModifierDefinition>& modifiers,
        std::mt19937& randomEngine)
    {
        ShopState generatedShop;
        std::mt19937 temporaryEngine = randomEngine;
        const CommandResult generationResult = generateOffers(
            player,
            generatedShop,
            gameConfig,
            units,
            faction,
            modifiers,
            temporaryEngine);
        if (!generationResult.success)
        {
            return generationResult;
        }

        shop = std::move(generatedShop);
        randomEngine = std::move(temporaryEngine);
        return success("商店已重建");
    }

    CommandResult ShopService::refresh(
        PlayerState& player,
        ShopState& shop,
        const GameConfig& gameConfig,
        const std::vector<UnitDefinition>& units,
        const FactionDefinition& faction,
        const std::vector<FactionModifierDefinition>& modifiers,
        std::mt19937& randomEngine)
    {
        if (gameConfig.shopRefreshCost < 0)
        {
            return fail(
                CommandErrorCode::InvalidConfiguration,
                "商店刷新费用不能为负数");
        }

        if (player.gold < 0)
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "玩家金币不能为负数");
        }

        if (player.gold < gameConfig.shopRefreshCost)
        {
            return fail(
                CommandErrorCode::InsufficientGold,
                "金币不足，无法刷新商店");
        }

        ShopState generatedShop;
        std::mt19937 temporaryEngine = randomEngine;
        const CommandResult generationResult = generateOffers(
            player,
            generatedShop,
            gameConfig,
            units,
            faction,
            modifiers,
            temporaryEngine);
        if (!generationResult.success)
        {
            return generationResult;
        }

        player.gold -= gameConfig.shopRefreshCost;
        shop = std::move(generatedShop);
        randomEngine = std::move(temporaryEngine);
        return success("商店刷新成功");
    }

    CommandResult ShopService::purchase(
        PlayerState& player,
        ShopState& shop,
        const std::size_t slot,
        const std::vector<UnitDefinition>& units,
        OwnedUnitId& nextOwnedUnitId)
    {
        std::string validationError;
        if (!PlayerStateService::validate(player, validationError))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "玩家状态无效：" + validationError);
        }

        if (slot >= shop.offers.size())
        {
            return fail(
                CommandErrorCode::InvalidShopSlot,
                "商店槽位编号无效");
        }

        if (!shop.offers[slot].has_value())
        {
            return fail(
                CommandErrorCode::EmptyShopSlot,
                "该商店槽位没有商品");
        }

        const ShopOffer& offer = shop.offers[slot].value();
        if (offer.displayedPrice <= 0)
        {
            return fail(
                CommandErrorCode::InvalidConfiguration,
                "商品显示价格必须为正数");
        }

        const UnitDefinition* selectedUnit = nullptr;
        for (const UnitDefinition& unit : units)
        {
            if (unit.id == offer.unitId)
            {
                selectedUnit = &unit;
                break;
            }
        }

        if (selectedUnit == nullptr)
        {
            return fail(
                CommandErrorCode::UnitNotFound,
                "商品对应的单位定义不存在");
        }

        if (PlayerStateService::activeCount(player)
            >= player.reserveSlots.size())
        {
            return fail(
                CommandErrorCode::RosterFull,
                "持有单位数量已达到上限");
        }

        if (player.gold < offer.displayedPrice)
        {
            return fail(
                CommandErrorCode::InsufficientGold,
                "金币不足，无法购买单位");
        }

        if (nextOwnedUnitId == InvalidOwnedUnitId
            || nextOwnedUnitId == std::numeric_limits<OwnedUnitId>::max()
            || containsOwnedUnitId(player, nextOwnedUnitId))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "下一个持久单位 ID 无效或已经被使用");
        }

        const std::optional<std::size_t> emptyReserveSlot =
            PlayerStateService::findFirstEmptyReserveSlot(player);
        if (!emptyReserveSlot.has_value())
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "玩家没有可用的备用区槽位");
        }

        PlayerState updatedPlayer = player;
        ShopState updatedShop = shop;
        const OwnedUnitId updatedNextOwnedUnitId = nextOwnedUnitId + 1;

        updatedPlayer.activeUnits.push_back(OwnedUnit{
            nextOwnedUnitId,
            UnitIdentity{selectedUnit->id, 1},
            player.side});
        updatedPlayer.reserveSlots[emptyReserveSlot.value()] =
            nextOwnedUnitId;
        updatedPlayer.gold -= offer.displayedPrice;
        updatedShop.offers[slot].reset();

        validationError.clear();
        if (!PlayerStateService::validate(updatedPlayer, validationError))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "购买后玩家状态无效：" + validationError);
        }

        player = std::move(updatedPlayer);
        shop = std::move(updatedShop);
        nextOwnedUnitId = updatedNextOwnedUnitId;
        return success("购买成功，单位已放入备用区");
    }
}
