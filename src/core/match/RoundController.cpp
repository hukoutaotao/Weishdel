#include "core/match/RoundController.hpp"

#include "core/economy/ShopService.hpp"
#include "core/economy/DeploymentService.hpp"
#include "core/economy/RosterService.hpp"
#include "core/model/PlayerStateService.hpp"

#include <algorithm>
#include <limits>
#include <set>
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

        // 此辅助函数把摘要列表加入集合并拒绝重复或无效持久 ID。
        bool appendUniqueIds(
            const std::vector<OwnedUnitId>& source,
            std::set<OwnedUnitId>& destination,
            std::string& errorMessage)
        {
            // 此循环逐一验证战斗摘要中的持久单位 ID。
            for (const OwnedUnitId unitId : source)
            {
                // 此分支拒绝无效 ID 或在多个结果分类中重复出现的 ID。
                if (unitId == InvalidOwnedUnitId
                    || !destination.insert(unitId).second)
                {
                    errorMessage = "战斗摘要包含无效或重复的持久单位 ID";
                    return false;
                }
            }

            return true;
        }

        // 此辅助函数收集双方实际参与战斗的全部部署单位 ID。
        std::set<OwnedUnitId> collectDeployedIds(
            const PlayerState& playerA,
            const PlayerState& playerB)
        {
            std::set<OwnedUnitId> deployedIds;
            // 此循环收集 A 方当前部署单位。
            for (const auto& deployment : playerA.deployments)
            {
                deployedIds.insert(deployment.second);
            }

            // 此循环收集 B 方当前部署单位。
            for (const auto& deployment : playerB.deployments)
            {
                deployedIds.insert(deployment.second);
            }

            return deployedIds;
        }

        // 此辅助函数确定一个持久单位唯一属于哪一方玩家。
        MapSide findUnitOwner(
            const PlayerState& playerA,
            const PlayerState& playerB,
            const OwnedUnitId unitId) noexcept
        {
            const bool belongsToA =
                PlayerStateService::findActive(playerA, unitId) != nullptr;
            const bool belongsToB =
                PlayerStateService::findActive(playerB, unitId) != nullptr;

            // 此分支只在单位恰好属于一方时返回有效阵营。
            if (belongsToA != belongsToB)
            {
                return belongsToA ? MapSide::A : MapSide::B;
            }

            return MapSide::Unknown;
        }

        // 此辅助函数把一个存活部署单位放回其玩家的首个空备用槽。
        CommandResult returnToReserve(
            PlayerState& player,
            const OwnedUnitId unitId)
        {
            const std::optional<std::size_t> emptySlot =
                PlayerStateService::findFirstEmptyReserveSlot(player);
            // 此分支拒绝违反持有容量约束的战后返回操作。
            if (!emptySlot.has_value())
            {
                return fail(
                    CommandErrorCode::RosterFull,
                    "战后没有空闲备用区槽位");
            }

            return DeploymentService::moveToReserve(
                player,
                unitId,
                emptySlot.value());
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

    // 此函数验证摘要完整性后批量回写死亡、存活位置和双方守卫值。
    CommandResult RoundController::settleBattle(
        PlayerState& playerA,
        PlayerState& playerB,
        const BattleSummary& battleSummary,
        const int roundNumber,
        RoundSummary& roundSummary)
    {
        // 此分支拒绝无效回合号和负数守卫伤害。
        if (roundNumber <= 0
            || battleSummary.guardDamageToA < 0
            || battleSummary.guardDamageToB < 0)
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "回合号和守卫伤害必须处于有效范围");
        }

        std::string validationError;
        // 此分支要求结算前双方玩家状态均有效且阵营正确。
        if (playerA.side != MapSide::A
            || !PlayerStateService::validate(playerA, validationError)
            || playerB.side != MapSide::B
            || !PlayerStateService::validate(playerB, validationError))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "结算前玩家状态无效：" + validationError);
        }

        std::set<OwnedUnitId> reportedIds;
        // 此分支要求死亡、到达守卫和普通存活列表中的 ID 全部唯一。
        if (!appendUniqueIds(
                battleSummary.deadOwnedUnitIds,
                reportedIds,
                validationError)
            || !appendUniqueIds(
                battleSummary.reachedGuardOwnedUnitIds,
                reportedIds,
                validationError)
            || !appendUniqueIds(
                battleSummary.survivingOwnedUnitIds,
                reportedIds,
                validationError))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                validationError);
        }

        const std::set<OwnedUnitId> deployedIds =
            collectDeployedIds(playerA, playerB);
        // 此分支要求战斗摘要恰好覆盖双方全部部署单位。
        if (reportedIds != deployedIds)
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "战斗摘要与实际部署单位不一致");
        }

        PlayerState updatedPlayerA = playerA;
        PlayerState updatedPlayerB = playerB;
        std::vector<OwnedUnitId> deadIds = battleSummary.deadOwnedUnitIds;
        std::sort(deadIds.begin(), deadIds.end());

        // 此循环按稳定 ID 顺序把死亡单位移入对应玩家的死亡列表。
        for (const OwnedUnitId unitId : deadIds)
        {
            const MapSide owner = findUnitOwner(
                updatedPlayerA,
                updatedPlayerB,
                unitId);
            // 此分支拒绝缺失或同时属于双方的持久单位 ID。
            if (owner == MapSide::Unknown)
            {
                return fail(
                    CommandErrorCode::InconsistentState,
                    "无法确定死亡单位所属玩家");
            }

            CommandResult result = owner == MapSide::A
                ? RosterService::markDead(updatedPlayerA, unitId)
                : RosterService::markDead(updatedPlayerB, unitId);
            // 此分支在死亡转移失败时保留调用前的双方状态。
            if (!result.success)
            {
                return result;
            }
        }

        std::vector<OwnedUnitId> livingIds =
            battleSummary.reachedGuardOwnedUnitIds;
        livingIds.insert(
            livingIds.end(),
            battleSummary.survivingOwnedUnitIds.begin(),
            battleSummary.survivingOwnedUnitIds.end());
        std::sort(livingIds.begin(), livingIds.end());

        // 此循环按持久 ID 顺序把全部战斗存活单位放回备用区。
        for (const OwnedUnitId unitId : livingIds)
        {
            const MapSide owner = findUnitOwner(
                updatedPlayerA,
                updatedPlayerB,
                unitId);
            // 此分支拒绝缺失或同时属于双方的存活单位 ID。
            if (owner == MapSide::Unknown)
            {
                return fail(
                    CommandErrorCode::InconsistentState,
                    "无法确定存活单位所属玩家");
            }

            CommandResult result = owner == MapSide::A
                ? returnToReserve(updatedPlayerA, unitId)
                : returnToReserve(updatedPlayerB, unitId);
            // 此分支在单位返回备用区失败时保留调用前的双方状态。
            if (!result.success)
            {
                return result;
            }
        }

        updatedPlayerA.guardValue = std::max(
            0,
            updatedPlayerA.guardValue - battleSummary.guardDamageToA);
        updatedPlayerB.guardValue = std::max(
            0,
            updatedPlayerB.guardValue - battleSummary.guardDamageToB);

        validationError.clear();
        // 此分支只允许完整有效的战后双方状态被提交。
        if (!PlayerStateService::validate(updatedPlayerA, validationError)
            || !PlayerStateService::validate(updatedPlayerB, validationError))
        {
            return fail(
                CommandErrorCode::InconsistentState,
                "结算后玩家状态无效：" + validationError);
        }

        RoundSummary updatedSummary;
        updatedSummary.roundNumber = roundNumber;
        updatedSummary.battle = battleSummary;
        updatedSummary.guardValueA = updatedPlayerA.guardValue;
        updatedSummary.guardValueB = updatedPlayerB.guardValue;
        // 此分支按双方承受的原始守卫伤害记录本回合败者。
        if (battleSummary.guardDamageToA
            > battleSummary.guardDamageToB)
        {
            updatedSummary.loserSide = MapSide::A;
        }
        else if (battleSummary.guardDamageToB
            > battleSummary.guardDamageToA)
        {
            updatedSummary.loserSide = MapSide::B;
        }

        playerA = std::move(updatedPlayerA);
        playerB = std::move(updatedPlayerB);
        roundSummary = std::move(updatedSummary);
        return CommandResult{
            true,
            CommandErrorCode::None,
            "回合结算成功"};
    }

    // 此函数优先处理守卫归零，再在最大回合后比较剩余守卫值。
    MatchResultSummary RoundController::determineMatchResult(
        const PlayerState& playerA,
        const PlayerState& playerB,
        const GameConfig& gameConfig,
        const int completedRounds) noexcept
    {
        MatchResultSummary result;
        result.completedRounds = completedRounds;
        result.guardValueA = playerA.guardValue;
        result.guardValueB = playerB.guardValue;

        // 此分支确保双方同回合守卫归零时优先判定为平局。
        if (playerA.guardValue <= 0 && playerB.guardValue <= 0)
        {
            result.outcome = MatchOutcome::Draw;
        }
        else if (playerA.guardValue <= 0)
        {
            result.outcome = MatchOutcome::SideBWin;
        }
        else if (playerB.guardValue <= 0)
        {
            result.outcome = MatchOutcome::SideAWin;
        }
        else if (completedRounds >= gameConfig.maxRounds)
        {
            // 此分支在最大回合结束后按剩余守卫值决定最终胜负。
            if (playerA.guardValue > playerB.guardValue)
            {
                result.outcome = MatchOutcome::SideAWin;
            }
            else if (playerB.guardValue > playerA.guardValue)
            {
                result.outcome = MatchOutcome::SideBWin;
            }
            else
            {
                result.outcome = MatchOutcome::Draw;
            }
        }

        return result;
    }
}
