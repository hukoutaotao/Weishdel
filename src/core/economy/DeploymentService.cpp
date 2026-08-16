#include "core/economy/DeploymentService.hpp"

#include "core/model/PlayerStateService.hpp"

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

        bool isLegalDeploymentStart(
            const MapDefinition& map,
            const MapSide side,
            const GridPosition target)
        {
            for (const Route& route : map.routes)
            {
                if (route.side == side && route.start == target)
                {
                    return true;
                }
            }

            return false;
        }

        CommandResult validatePlayer(
            const PlayerState& player,
            std::string& validationError)
        {
            validationError.clear();
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
            if (faction.id.empty() || faction.maxDeployed < 0)
            {
                return fail(
                    CommandErrorCode::InvalidConfiguration,
                    "分队部署上限配置无效");
            }

            if (player.factionId.empty() || player.factionId != faction.id)
            {
                return fail(
                    CommandErrorCode::InconsistentState,
                    "玩家分队与部署分队不一致");
            }

            return success("分队配置有效");
        }
    }

    CommandResult DeploymentService::moveToDeployment(
        PlayerState& player,
        const MapDefinition& map,
        const FactionDefinition& faction,
        const OwnedUnitId unitId,
        const GridPosition target)
    {
        std::string validationError;
        const CommandResult playerResult =
            validatePlayer(player, validationError);
        if (!playerResult.success)
        {
            return playerResult;
        }

        const CommandResult factionResult = validateFaction(player, faction);
        if (!factionResult.success)
        {
            return factionResult;
        }

        const OwnedUnit* unit = PlayerStateService::findActive(player, unitId);
        if (unit == nullptr)
        {
            return fail(
                CommandErrorCode::UnitNotFound,
                "找不到可部署的活动单位");
        }

        if (unit->ownerSide != player.side)
        {
            return fail(
                CommandErrorCode::WrongOwner,
                "单位所属阵营与玩家不一致");
        }

        if (!isLegalDeploymentStart(map, player.side, target))
        {
            return fail(
                CommandErrorCode::InvalidTarget,
                "目标不是当前玩家的合法部署起点");
        }

        const std::optional<GridPosition> currentPosition =
            PlayerStateService::findDeploymentPosition(player, unitId);
        if (currentPosition.has_value() && currentPosition.value() == target)
        {
            return fail(
                CommandErrorCode::InvalidTarget,
                "单位已经位于目标部署格");
        }

        if (player.deployments.find(target) != player.deployments.end())
        {
            return fail(
                CommandErrorCode::TargetOccupied,
                "目标部署格已经被占用");
        }

        const std::optional<std::size_t> currentReserveSlot =
            PlayerStateService::findReserveSlot(player, unitId);
        if (!currentPosition.has_value() && !currentReserveSlot.has_value())
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "活动单位没有唯一的位置");
        }

        if (currentReserveSlot.has_value()
            && player.deployments.size()
                >= static_cast<std::size_t>(faction.maxDeployed))
        {
            return fail(
                CommandErrorCode::DeploymentLimitReached,
                "已达到分队部署数量上限");
        }

        PlayerState updatedPlayer = player;
        if (currentReserveSlot.has_value())
        {
            updatedPlayer.reserveSlots[currentReserveSlot.value()].reset();
        }
        else
        {
            updatedPlayer.deployments.erase(currentPosition.value());
        }
        updatedPlayer.deployments.emplace(target, unitId);

        validationError.clear();
        if (!PlayerStateService::validate(updatedPlayer, validationError))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "部署后玩家状态无效：" + validationError);
        }

        player = std::move(updatedPlayer);
        return success("单位已部署");
    }

    CommandResult DeploymentService::moveToReserve(
        PlayerState& player,
        const OwnedUnitId unitId,
        const std::size_t targetSlot)
    {
        std::string validationError;
        const CommandResult playerResult =
            validatePlayer(player, validationError);
        if (!playerResult.success)
        {
            return playerResult;
        }

        const OwnedUnit* unit = PlayerStateService::findActive(player, unitId);
        if (unit == nullptr)
        {
            return fail(
                CommandErrorCode::UnitNotFound,
                "找不到可撤回的活动单位");
        }

        if (targetSlot >= player.reserveSlots.size())
        {
            return fail(
                CommandErrorCode::InvalidTarget,
                "备用区槽位编号无效");
        }

        const std::optional<std::size_t> currentReserveSlot =
            PlayerStateService::findReserveSlot(player, unitId);
        if (currentReserveSlot.has_value()
            && currentReserveSlot.value() == targetSlot)
        {
            return fail(
                CommandErrorCode::InvalidTarget,
                "单位已经位于目标备用区槽位");
        }

        if (player.reserveSlots[targetSlot].has_value())
        {
            return fail(
                CommandErrorCode::TargetOccupied,
                "目标备用区槽位已经被占用");
        }

        const std::optional<GridPosition> currentPosition =
            PlayerStateService::findDeploymentPosition(player, unitId);
        if (!currentReserveSlot.has_value() && !currentPosition.has_value())
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "活动单位没有唯一的位置");
        }

        PlayerState updatedPlayer = player;
        if (currentReserveSlot.has_value())
        {
            updatedPlayer.reserveSlots[currentReserveSlot.value()].reset();
        }
        else
        {
            updatedPlayer.deployments.erase(currentPosition.value());
        }
        updatedPlayer.reserveSlots[targetSlot] = unitId;

        validationError.clear();
        if (!PlayerStateService::validate(updatedPlayer, validationError))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "撤回后玩家状态无效：" + validationError);
        }

        player = std::move(updatedPlayer);
        return success("单位已移回备用区");
    }
}
