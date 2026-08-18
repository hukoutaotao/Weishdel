#include "core/match/RoundController.hpp"

#include "core/economy/ShopService.hpp"
#include "core/model/PlayerStateService.hpp"

#include <limits>
#include <string>
#include <utility>

// 此命名空间实现回合边界上的原子状态变更。
namespace autochess::core
{
    // 此匿名命名空间保存仅供回合控制器使用的校验辅助函数。
    namespace
    {
        // 此辅助函数构造带中文说明的失败结果。
        CommandResult fail(
            const CommandErrorCode errorCode,
            std::string message)
        {
            return CommandResult{false, errorCode, std::move(message)};
        }

        // 此辅助函数构造准备阶段成功结果。
        CommandResult success()
        {
            return CommandResult{
                true,
                CommandErrorCode::None,
                "准备阶段初始化成功"};
        }

        // 此辅助函数按 ID 查找玩家当前选择的分队定义。
        const FactionDefinition* findFaction(
            const std::vector<FactionDefinition>& factions,
            const std::string& factionId) noexcept
        {
            // 此循环在已校验配置中查找唯一的分队 ID。
            for (const FactionDefinition& faction : factions)
            {
                // 此分支在找到目标分队时立即返回只读定义。
                if (faction.id == factionId)
                {
                    return &faction;
                }
            }

            return nullptr;
        }

        // 此辅助函数检查并发放不会造成整数溢出的金币收入。
        bool addIncome(
            PlayerState& player,
            const long long amount,
            std::string& errorMessage)
        {
            const long long updatedGold =
                static_cast<long long>(player.gold) + amount;

            // 此分支拒绝超出玩家金币整数范围的回合收入。
            if (updatedGold < 0
                || updatedGold > std::numeric_limits<int>::max())
            {
                errorMessage = "发放回合收入后金币超出有效范围";
                return false;
            }

            player.gold = static_cast<int>(updatedGold);
            return true;
        }
    }

    // 此函数以副本完成收入、补助和商店重建后再一次性提交状态。
    CommandResult RoundController::beginPreparation(
        PlayerState& playerA,
        PlayerState& playerB,
        ShopState& shopA,
        ShopState& shopB,
        const GameConfig& gameConfig,
        const std::vector<UnitDefinition>& units,
        const std::vector<FactionDefinition>& factions,
        const std::vector<FactionModifierDefinition>& modifiers,
        const MapSide previousLoser,
        std::mt19937& randomEngine)
    {
        // 此分支拒绝无法安全发放的负数经济配置。
        if (gameConfig.roundIncome < 0 || gameConfig.loserBonus < 0)
        {
            return fail(
                CommandErrorCode::InvalidConfiguration,
                "回合收入和败者补助不能为负数");
        }

        // 此分支拒绝未知值以外的非法败者阵营。
        if (previousLoser != MapSide::Unknown
            && previousLoser != MapSide::A
            && previousLoser != MapSide::B)
        {
            return fail(
                CommandErrorCode::InvalidActor,
                "上一回合败者阵营无效");
        }

        std::string validationError;
        // 此分支要求传入的 A 方玩家状态完整且阵营正确。
        if (playerA.side != MapSide::A
            || !PlayerStateService::validate(playerA, validationError))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "A方玩家状态无效：" + validationError);
        }

        validationError.clear();
        // 此分支要求传入的 B 方玩家状态完整且阵营正确。
        if (playerB.side != MapSide::B
            || !PlayerStateService::validate(playerB, validationError))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "B方玩家状态无效：" + validationError);
        }

        const FactionDefinition* factionA =
            findFaction(factions, playerA.factionId);
        const FactionDefinition* factionB =
            findFaction(factions, playerB.factionId);
        // 此分支拒绝玩家引用不存在的分队定义。
        if (factionA == nullptr || factionB == nullptr)
        {
            return fail(
                CommandErrorCode::InvalidConfiguration,
                "玩家选择的分队定义不存在");
        }

        PlayerState updatedPlayerA = playerA;
        PlayerState updatedPlayerB = playerB;
        ShopState updatedShopA;
        ShopState updatedShopB;
        std::mt19937 updatedRandomEngine = randomEngine;

        const long long incomeA =
            static_cast<long long>(gameConfig.roundIncome)
            + (previousLoser == MapSide::A ? gameConfig.loserBonus : 0);
        const long long incomeB =
            static_cast<long long>(gameConfig.roundIncome)
            + (previousLoser == MapSide::B ? gameConfig.loserBonus : 0);

        // 此分支保证双方金币都能安全增加后才继续生成商店。
        if (!addIncome(updatedPlayerA, incomeA, validationError)
            || !addIncome(updatedPlayerB, incomeB, validationError))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                validationError);
        }

        const CommandResult rebuildA = ShopService::rebuild(
            updatedPlayerA,
            updatedShopA,
            gameConfig,
            units,
            *factionA,
            modifiers,
            updatedRandomEngine);
        // 此分支在 A 方商店生成失败时保留调用前的全部状态。
        if (!rebuildA.success)
        {
            return rebuildA;
        }

        const CommandResult rebuildB = ShopService::rebuild(
            updatedPlayerB,
            updatedShopB,
            gameConfig,
            units,
            *factionB,
            modifiers,
            updatedRandomEngine);
        // 此分支在 B 方商店生成失败时保留调用前的全部状态。
        if (!rebuildB.success)
        {
            return rebuildB;
        }

        playerA = std::move(updatedPlayerA);
        playerB = std::move(updatedPlayerB);
        shopA = std::move(updatedShopA);
        shopB = std::move(updatedShopB);
        randomEngine = std::move(updatedRandomEngine);
        return success();
    }
}
