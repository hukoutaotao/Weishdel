#include "core/economy/MergeService.hpp"

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
        // 此函数统一构造带中文说明的合成失败结果。
        CommandResult fail(
            const CommandErrorCode errorCode,
            const std::string& message)
        {
            CommandResult result;
            result.errorCode = errorCode;
            result.message = message;
            return result;
        }

        // 此函数根据合成后的等级构造成功结果。
        CommandResult success(const int newLevel)
        {
            CommandResult result;
            result.success = true;
            result.errorCode = CommandErrorCode::None;
            result.message = "合成成功，单位已提升至 "
                + std::to_string(newLevel) + " 级";
            return result;
        }

        // 此函数检查候选持久 ID 是否已被活动或死亡单位使用。
        bool containsOwnedUnitId(
            const PlayerState& player,
            const OwnedUnitId unitId)
        {
            return PlayerStateService::findActive(player, unitId) != nullptr
                || PlayerStateService::findDead(player, unitId) != nullptr;
        }

        // 此函数按单位类型 ID 查找合成价格所需的单位定义。
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
    }

    CommandResult MergeService::merge(
        PlayerState& player,
        const OwnedUnitId sourceUnitId,
        const OwnedUnitId targetUnitId,
        const GameConfig& gameConfig,
        const std::vector<UnitDefinition>& units,
        const FactionDefinition& faction,
        const std::vector<FactionModifierDefinition>& modifiers,
        OwnedUnitId& nextOwnedUnitId)
    {
        // 此代码段先拒绝任何不满足玩家状态不变量的合成请求。
        std::string validationError;
        if (!PlayerStateService::validate(player, validationError))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "玩家状态无效：" + validationError);
        }

        // 此代码段确认玩家和价格计算使用的是同一个分队。
        if (player.factionId.empty()
            || faction.id.empty()
            || player.factionId != faction.id)
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "玩家分队与合成价格分队不一致");
        }

        // 此代码段拒绝无法产生更高等级单位的最高等级配置。
        if (gameConfig.maxUnitLevel < 2)
        {
            return fail(
                CommandErrorCode::InvalidConfiguration,
                "单位最高等级配置必须至少为 2");
        }

        // 此代码段阻止调用方用同一个单位 ID 作为合成双方。
        if (sourceUnitId == targetUnitId)
        {
            return fail(
                CommandErrorCode::NotMergeable,
                "同一个单位不能与自己合成");
        }

        // 此代码段只允许两个仍在活动列表中的单位参与合成。
        const OwnedUnit* sourceUnit =
            PlayerStateService::findActive(player, sourceUnitId);
        const OwnedUnit* targetUnit =
            PlayerStateService::findActive(player, targetUnitId);
        if (sourceUnit == nullptr || targetUnit == nullptr)
        {
            return fail(
                CommandErrorCode::UnitNotFound,
                "找不到参与合成的活动单位");
        }

        // 此代码段使用 UnitIdentity 限定同类型且同等级才能合成。
        if (!(sourceUnit->identity == targetUnit->identity))
        {
            return fail(
                CommandErrorCode::NotMergeable,
                "只有类型和等级完全相同的单位才能合成");
        }

        // 此代码段按配置的最高等级阻止满级单位继续升级。
        if (sourceUnit->identity.level >= gameConfig.maxUnitLevel)
        {
            return fail(
                CommandErrorCode::MaxLevelReached,
                "单位已经达到最高等级，不能继续合成");
        }

        // 此代码段查找单位定义，以便计算经过分队修正的一级价格。
        const UnitDefinition* unitDefinition = findUnitDefinition(
            units, sourceUnit->identity.unitId);
        if (unitDefinition == nullptr)
        {
            return fail(
                CommandErrorCode::InvalidConfiguration,
                "参与合成的单位定义不存在");
        }

        // 此代码段计算合成返还所依赖的修正后一级购买价。
        const PriceCalculationResult levelOnePrice =
            PriceRules::calculateLevelOnePrice(
                *unitDefinition, faction, modifiers);
        if (!levelOnePrice.success)
        {
            return fail(
                CommandErrorCode::InvalidConfiguration,
                "无法计算合成单位价格：" + levelOnePrice.message);
        }

        // 此代码段按游戏配置的合成比例计算整数返还金额。
        const PriceCalculationResult refund =
            PriceRules::calculateRatioAmount(
                levelOnePrice.amount,
                gameConfig.mergeRefundRatio);
        if (!refund.success)
        {
            return fail(
                CommandErrorCode::InvalidConfiguration,
                "无法计算合成返还：" + refund.message);
        }

        // 此代码段阻止返还金币时发生有符号整数溢出。
        if (player.gold > std::numeric_limits<int>::max() - refund.amount)
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "合成返还后金币将超出整数范围");
        }

        // 此代码段保证新单位 ID 有效、可递增且未被任何持久单位占用。
        if (nextOwnedUnitId == InvalidOwnedUnitId
            || nextOwnedUnitId == std::numeric_limits<OwnedUnitId>::max()
            || containsOwnedUnitId(player, nextOwnedUnitId))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "下一个持久单位 ID 无效或已经被使用");
        }

        // 此代码段记录目标单位的唯一位置，供新单位在合成后继承。
        const std::optional<std::size_t> targetReserveSlot =
            PlayerStateService::findReserveSlot(player, targetUnitId);
        const std::optional<GridPosition> targetDeploymentPosition =
            PlayerStateService::findDeploymentPosition(
                player, targetUnitId);
        if (targetReserveSlot.has_value()
            == targetDeploymentPosition.has_value())
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "目标单位没有唯一位置");
        }

        // 此代码段复制玩家状态并从活动列表移除两个原单位。
        PlayerState updatedPlayer = player;
        updatedPlayer.activeUnits.erase(
            std::remove_if(
                updatedPlayer.activeUnits.begin(),
                updatedPlayer.activeUnits.end(),
                [sourceUnitId, targetUnitId](const OwnedUnit& unit)
                {
                    return unit.id == sourceUnitId
                        || unit.id == targetUnitId;
                }),
            updatedPlayer.activeUnits.end());

        // 此代码段清除两个原单位占用的所有备用区槽位。
        for (std::optional<OwnedUnitId>& slot : updatedPlayer.reserveSlots)
        {
            if (slot.has_value()
                && (slot.value() == sourceUnitId
                    || slot.value() == targetUnitId))
            {
                slot.reset();
            }
        }

        // 此代码段清除两个原单位对应的部署映射。
        for (auto deployment = updatedPlayer.deployments.begin();
             deployment != updatedPlayer.deployments.end();)
        {
            if (deployment->second == sourceUnitId
                || deployment->second == targetUnitId)
            {
                deployment = updatedPlayer.deployments.erase(deployment);
            }
            else
            {
                ++deployment;
            }
        }

        // 此代码段使用新持久 ID 创建高一级活动单位。
        const int newLevel = sourceUnit->identity.level + 1;
        updatedPlayer.activeUnits.push_back(OwnedUnit{
            nextOwnedUnitId,
            UnitIdentity{sourceUnit->identity.unitId, newLevel},
            player.side});

        // 此代码段把新单位放回目标单位原来的备用区槽位或部署格。
        if (targetReserveSlot.has_value())
        {
            updatedPlayer.reserveSlots[targetReserveSlot.value()] =
                nextOwnedUnitId;
        }
        else
        {
            updatedPlayer.deployments.emplace(
                targetDeploymentPosition.value(), nextOwnedUnitId);
        }

        // 此代码段更新返还金币和下一持久 ID 的临时结果。
        updatedPlayer.gold += refund.amount;
        const OwnedUnitId updatedNextOwnedUnitId = nextOwnedUnitId + 1;

        // 此代码段在提交前再次验证合成后的玩家状态不变量。
        validationError.clear();
        if (!PlayerStateService::validate(updatedPlayer, validationError))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "合成后玩家状态无效：" + validationError);
        }

        // 此代码段只在全部校验通过后一次性提交玩家状态和下一 ID。
        player = std::move(updatedPlayer);
        nextOwnedUnitId = updatedNextOwnedUnitId;
        return success(newLevel);
    }
}
