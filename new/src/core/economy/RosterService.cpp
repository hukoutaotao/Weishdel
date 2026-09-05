#include "core/economy/RosterService.hpp"

#include "core/economy/PriceRules.hpp"
#include "core/model/PlayerStateService.hpp"

#include <algorithm>
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

        CommandResult success(const std::string& message)
        {
            CommandResult result;
            result.success = true;
            result.errorCode = CommandErrorCode::None;
            result.message = message;
            return result;
        }

        const UnitDefinition* findUnitDefinition(
            const std::vector<UnitDefinition>& units,
            const std::string& unitId)
        {
            for (const UnitDefinition& unit : units)
            {
                if (unit.id == unitId)
                {
                    return &unit;
                }
            }

            return nullptr;
        }

        CommandResult validatePlayer(const PlayerState& player)
        {
            std::string validationError;
            if (!PlayerStateService::validate(player, validationError))
            {
                return fail(
                    CommandErrorCode::InconsistentState,
                    "玩家状态无效：" + validationError);
            }

            return success("玩家状态有效");
        }

        CommandResult validateFaction(
            const PlayerState& player,
            const FactionDefinition& faction)
        {
            if (faction.id.empty())
            {
                return fail(
                    CommandErrorCode::InvalidConfiguration,
                    "分队定义 ID 不能为空");
            }

            if (player.factionId.empty() || player.factionId != faction.id)
            {
                return fail(
                    CommandErrorCode::InconsistentState,
                    "玩家分队与单位价格分队不一致");
            }

            return success("分队配置有效");
        }

        bool findUniqueLocation(
            const PlayerState& player,
            const OwnedUnitId unitId,
            std::optional<std::size_t>& reserveSlot,
            std::optional<GridPosition>& deploymentPosition)
        {
            reserveSlot = PlayerStateService::findReserveSlot(player, unitId);
            deploymentPosition =
                PlayerStateService::findDeploymentPosition(player, unitId);
            return reserveSlot.has_value()
                != deploymentPosition.has_value();
        }

        void removeActiveUnitAndLocation(
            PlayerState& player,
            const OwnedUnitId unitId,
            const std::optional<std::size_t>& reserveSlot,
            const std::optional<GridPosition>& deploymentPosition)
        {
            player.activeUnits.erase(
                std::remove_if(
                    player.activeUnits.begin(),
                    player.activeUnits.end(),
                    [unitId](const OwnedUnit& unit)
                    {
                        return unit.id == unitId;
                    }),
                player.activeUnits.end());

            if (reserveSlot.has_value())
            {
                player.reserveSlots[reserveSlot.value()].reset();
            }
            else
            {
                player.deployments.erase(deploymentPosition.value());
            }
        }
    }

    CommandResult RosterService::sell(
        PlayerState& player,
        const OwnedUnitId unitId,
        const GameConfig& gameConfig,
        const std::vector<UnitDefinition>& units,
        const FactionDefinition& faction,
        const std::vector<FactionModifierDefinition>& modifiers)
    {
        const CommandResult playerResult = validatePlayer(player);
        if (!playerResult.success)
        {
            return playerResult;
        }

        const OwnedUnit* activeUnit =
            PlayerStateService::findActive(player, unitId);
        if (activeUnit == nullptr)
        {
            if (PlayerStateService::findDead(player, unitId) != nullptr)
            {
                return fail(
                    CommandErrorCode::UnitAlreadyDead,
                    "死亡单位不能出售");
            }

            return fail(
                CommandErrorCode::UnitNotFound,
                "找不到可出售的活动单位");
        }

        const CommandResult factionResult = validateFaction(player, faction);
        if (!factionResult.success)
        {
            return factionResult;
        }

        const UnitDefinition* unitDefinition = findUnitDefinition(
            units, activeUnit->identity.unitId);
        if (unitDefinition == nullptr)
        {
            return fail(
                CommandErrorCode::InvalidConfiguration,
                "出售单位对应的单位定义不存在");
        }

        const PriceCalculationResult levelOnePrice =
            PriceRules::calculateLevelOnePrice(
                *unitDefinition, faction, modifiers);
        if (!levelOnePrice.success)
        {
            return fail(
                CommandErrorCode::InvalidConfiguration,
                "无法计算出售单位价格：" + levelOnePrice.message);
        }

        const PriceCalculationResult refund =
            PriceRules::calculateRatioAmount(
                levelOnePrice.amount, gameConfig.sellRatio);
        if (!refund.success)
        {
            return fail(
                CommandErrorCode::InvalidConfiguration,
                "无法计算出售返还：" + refund.message);
        }

        if (player.gold > std::numeric_limits<int>::max() - refund.amount)
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "出售返还后金币将超出整数范围");
        }

        std::optional<std::size_t> reserveSlot;
        std::optional<GridPosition> deploymentPosition;
        if (!findUniqueLocation(
                player, unitId, reserveSlot, deploymentPosition))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "出售单位没有唯一位置");
        }

        PlayerState updatedPlayer = player;
        removeActiveUnitAndLocation(
            updatedPlayer, unitId, reserveSlot, deploymentPosition);
        updatedPlayer.gold += refund.amount;

        std::string validationError;
        if (!PlayerStateService::validate(updatedPlayer, validationError))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "出售后玩家状态无效：" + validationError);
        }

        player = std::move(updatedPlayer);
        return success(
            "出售成功，返还 " + std::to_string(refund.amount) + " 金币");
    }

    CommandResult RosterService::markDead(
        PlayerState& player,
        const OwnedUnitId unitId)
    {
        const CommandResult playerResult = validatePlayer(player);
        if (!playerResult.success)
        {
            return playerResult;
        }

        const OwnedUnit* activeUnit =
            PlayerStateService::findActive(player, unitId);
        if (activeUnit == nullptr)
        {
            if (PlayerStateService::findDead(player, unitId) != nullptr)
            {
                return fail(
                    CommandErrorCode::UnitAlreadyDead,
                    "单位已经处于死亡列表");
            }

            return fail(
                CommandErrorCode::UnitNotFound,
                "找不到需要转入死亡列表的活动单位");
        }

        std::optional<std::size_t> reserveSlot;
        std::optional<GridPosition> deploymentPosition;
        if (!findUniqueLocation(
                player, unitId, reserveSlot, deploymentPosition))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "死亡单位转移前没有唯一位置");
        }

        const OwnedUnit deadUnit = *activeUnit;
        PlayerState updatedPlayer = player;
        removeActiveUnitAndLocation(
            updatedPlayer, unitId, reserveSlot, deploymentPosition);
        updatedPlayer.deadUnits.push_back(deadUnit);

        std::string validationError;
        if (!PlayerStateService::validate(updatedPlayer, validationError))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "死亡转移后玩家状态无效：" + validationError);
        }

        player = std::move(updatedPlayer);
        return success("单位已移入死亡列表");
    }

    CommandResult RosterService::revive(
        PlayerState& player,
        const OwnedUnitId unitId,
        const GameConfig& gameConfig,
        const std::vector<UnitDefinition>& units,
        const FactionDefinition& faction,
        const std::vector<FactionModifierDefinition>& modifiers)
    {
        const CommandResult playerResult = validatePlayer(player);
        if (!playerResult.success)
        {
            return playerResult;
        }

        const OwnedUnit* deadUnit =
            PlayerStateService::findDead(player, unitId);
        if (deadUnit == nullptr)
        {
            if (PlayerStateService::findActive(player, unitId) != nullptr)
            {
                return fail(
                    CommandErrorCode::UnitAlreadyActive,
                    "活动单位不需要复活");
            }

            return fail(
                CommandErrorCode::UnitNotFound,
                "找不到需要复活的死亡单位");
        }

        const CommandResult factionResult = validateFaction(player, faction);
        if (!factionResult.success)
        {
            return factionResult;
        }

        if (PlayerStateService::activeCount(player)
            >= player.reserveSlots.size())
        {
            return fail(
                CommandErrorCode::RosterFull,
                "持有单位数量已达到上限，无法复活");
        }

        const std::optional<std::size_t> emptyReserveSlot =
            PlayerStateService::findFirstEmptyReserveSlot(player);
        if (!emptyReserveSlot.has_value())
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "没有可用于复活单位的备用区槽位");
        }

        const UnitDefinition* unitDefinition = findUnitDefinition(
            units, deadUnit->identity.unitId);
        if (unitDefinition == nullptr)
        {
            return fail(
                CommandErrorCode::InvalidConfiguration,
                "复活单位对应的单位定义不存在");
        }

        const PriceCalculationResult levelOnePrice =
            PriceRules::calculateLevelOnePrice(
                *unitDefinition, faction, modifiers);
        if (!levelOnePrice.success)
        {
            return fail(
                CommandErrorCode::InvalidConfiguration,
                "无法计算复活单位价格：" + levelOnePrice.message);
        }

        const PriceCalculationResult reviveCost =
            PriceRules::calculateRatioAmount(
                levelOnePrice.amount, gameConfig.reviveRatio);
        if (!reviveCost.success)
        {
            return fail(
                CommandErrorCode::InvalidConfiguration,
                "无法计算复活费用：" + reviveCost.message);
        }

        if (player.gold < reviveCost.amount)
        {
            return fail(
                CommandErrorCode::InsufficientGold,
                "金币不足，无法复活单位");
        }

        const OwnedUnit revivedUnit = *deadUnit;
        PlayerState updatedPlayer = player;
        updatedPlayer.deadUnits.erase(
            std::remove_if(
                updatedPlayer.deadUnits.begin(),
                updatedPlayer.deadUnits.end(),
                [unitId](const OwnedUnit& unit)
                {
                    return unit.id == unitId;
                }),
            updatedPlayer.deadUnits.end());
        updatedPlayer.activeUnits.push_back(revivedUnit);
        updatedPlayer.reserveSlots[emptyReserveSlot.value()] = unitId;
        updatedPlayer.gold -= reviveCost.amount;

        std::string validationError;
        if (!PlayerStateService::validate(updatedPlayer, validationError))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "复活后玩家状态无效：" + validationError);
        }

        player = std::move(updatedPlayer);
        return success(
            "复活成功，消耗 " + std::to_string(reviveCost.amount)
            + " 金币，单位已进入备用区");
    }
}
