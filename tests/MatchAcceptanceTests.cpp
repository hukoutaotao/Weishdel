#include "MatchAcceptanceTests.hpp"

#include "core/config/ConfigBundleLoader.hpp"
#include "core/match/Match.hpp"
#include "core/match/RoundController.hpp"
#include "core/model/PlayerStateService.hpp"

#include <iostream>
#include <random>
#include <string>
#include <utility>

// 此匿名命名空间保存第六天最终验收矩阵使用的检查工具。
namespace
{
    // 此测试运行器统一输出验收矩阵结果并累计失败数量。
    class AcceptanceRunner
    {
    public:
        // 此函数根据条件输出通过或失败并更新计数。
        void check(const bool condition, const std::string& name)
        {
            // 此分支输出通过项目且保持失败计数不变。
            if (condition)
            {
                std::cout << "[PASS] " << name << '\n';
                return;
            }

            std::cerr << "[FAIL] " << name << '\n';
            ++failureCount_;
        }

        // 此函数返回验收矩阵累计失败数量。
        int failureCount() const noexcept
        {
            return failureCount_;
        }

    private:
        int failureCount_ = 0;
    };

    // 此辅助函数按 ID 查找正式分队定义。
    const autochess::core::FactionDefinition* findFaction(
        const autochess::core::ConfigBundle& bundle,
        const std::string& factionId) noexcept
    {
        // 此循环在正式分队列表中查找测试需要的目标 ID。
        for (const autochess::core::FactionDefinition& faction
             : bundle.factions)
        {
            // 此分支在找到匹配分队时立即返回只读地址。
            if (faction.id == factionId)
            {
                return &faction;
            }
        }

        return nullptr;
    }

    // 此辅助函数比较商店槽位内容以验证失败命令没有修改商品。
    bool shopsEqual(
        const autochess::core::ShopState& left,
        const autochess::core::ShopState& right)
    {
        // 此分支拒绝槽位数量不同的商店状态。
        if (left.offers.size() != right.offers.size())
        {
            return false;
        }

        // 此循环逐槽比较空值、单位 ID 和显示价格。
        for (std::size_t slot = 0; slot < left.offers.size(); ++slot)
        {
            // 此分支拒绝空槽状态不同的对应槽位。
            if (left.offers[slot].has_value()
                != right.offers[slot].has_value())
            {
                return false;
            }

            // 此分支拒绝任一非空槽位的商品内容变化。
            if (left.offers[slot].has_value()
                && (left.offers[slot]->unitId
                        != right.offers[slot]->unitId
                    || left.offers[slot]->displayedPrice
                        != right.offers[slot]->displayedPrice))
            {
                return false;
            }
        }

        return true;
    }

    // 此函数验证双方败者补助和平局无补助的精确金币序列。
    void checkRoundIncomeMatrix(
        AcceptanceRunner& runner,
        const autochess::core::ConfigBundle& bundle)
    {
        const auto* factionA = findFaction(bundle, "training_team");
        const auto* factionB = findFaction(bundle, "assault_team");
        runner.check(
            factionA != nullptr && factionB != nullptr,
            "Acceptance matrix finds both factions");
        // 此分支在测试分队缺失时停止收入矩阵构造。
        if (factionA == nullptr || factionB == nullptr)
        {
            return;
        }

        auto playerA = autochess::core::PlayerStateService::createInitial(
            autochess::core::MapSide::A,
            bundle.gameConfig,
            *factionA);
        auto playerB = autochess::core::PlayerStateService::createInitial(
            autochess::core::MapSide::B,
            bundle.gameConfig,
            *factionB);
        autochess::core::ShopState shopA;
        autochess::core::ShopState shopB;
        std::mt19937 randomEngine(bundle.gameConfig.randomSeed);

        const auto first = autochess::core::RoundController::beginPreparation(
            playerA,
            playerB,
            shopA,
            shopB,
            bundle.gameConfig,
            bundle.units,
            bundle.factions,
            bundle.factionModifiers,
            autochess::core::MapSide::Unknown,
            randomEngine);
        const auto second = autochess::core::RoundController::beginPreparation(
            playerA,
            playerB,
            shopA,
            shopB,
            bundle.gameConfig,
            bundle.units,
            bundle.factions,
            bundle.factionModifiers,
            autochess::core::MapSide::B,
            randomEngine);
        runner.check(
            first.success
                && second.success
                && playerA.gold == 20
                && playerB.gold == 22,
            "Acceptance matrix grants loser bonus only to side B");

        const auto third = autochess::core::RoundController::beginPreparation(
            playerA,
            playerB,
            shopA,
            shopB,
            bundle.gameConfig,
            bundle.units,
            bundle.factions,
            bundle.factionModifiers,
            autochess::core::MapSide::Unknown,
            randomEngine);
        runner.check(
            third.success
                && playerA.gold == 25
                && playerB.gold == 27,
            "Acceptance matrix grants no bonus after a tied round");
    }

    // 此函数验证守卫归零优先级和最大回合比较的全部结果分支。
    void checkOutcomeMatrix(
        AcceptanceRunner& runner,
        const autochess::core::ConfigBundle& bundle)
    {
        autochess::core::PlayerState playerA;
        playerA.side = autochess::core::MapSide::A;
        playerA.guardValue = 10;
        autochess::core::PlayerState playerB;
        playerB.side = autochess::core::MapSide::B;
        playerB.guardValue = 10;

        playerA.guardValue = 0;
        runner.check(
            autochess::core::RoundController::determineMatchResult(
                playerA,
                playerB,
                bundle.gameConfig,
                1).outcome == autochess::core::MatchOutcome::SideBWin,
            "Acceptance matrix awards B when A guard reaches zero");

        playerA.guardValue = 10;
        playerB.guardValue = 0;
        runner.check(
            autochess::core::RoundController::determineMatchResult(
                playerA,
                playerB,
                bundle.gameConfig,
                1).outcome == autochess::core::MatchOutcome::SideAWin,
            "Acceptance matrix awards A when B guard reaches zero");

        playerA.guardValue = 0;
        runner.check(
            autochess::core::RoundController::determineMatchResult(
                playerA,
                playerB,
                bundle.gameConfig,
                1).outcome == autochess::core::MatchOutcome::Draw,
            "Acceptance matrix draws simultaneous guard depletion");

        playerA.guardValue = 60;
        playerB.guardValue = 40;
        runner.check(
            autochess::core::RoundController::determineMatchResult(
                playerA,
                playerB,
                bundle.gameConfig,
                2).outcome == autochess::core::MatchOutcome::Ongoing
                && autochess::core::RoundController::determineMatchResult(
                    playerA,
                    playerB,
                    bundle.gameConfig,
                    3).outcome == autochess::core::MatchOutcome::SideAWin,
            "Acceptance matrix waits then awards higher side A guard");

        playerA.guardValue = 30;
        playerB.guardValue = 50;
        runner.check(
            autochess::core::RoundController::determineMatchResult(
                playerA,
                playerB,
                bundle.gameConfig,
                3).outcome == autochess::core::MatchOutcome::SideBWin,
            "Acceptance matrix awards higher side B guard at round three");

        playerA.guardValue = 50;
        runner.check(
            autochess::core::RoundController::determineMatchResult(
                playerA,
                playerB,
                bundle.gameConfig,
                3).outcome == autochess::core::MatchOutcome::Draw,
            "Acceptance matrix draws equal guards at round three");
    }

    // 此函数验证同时归零结算仍按原始伤害记录败者并最终判平。
    void checkSimultaneousSettlement(
        AcceptanceRunner& runner,
        const autochess::core::ConfigBundle& bundle)
    {
        const auto* factionA = findFaction(bundle, "training_team");
        const auto* factionB = findFaction(bundle, "assault_team");
        // 此分支在测试分队缺失时停止同时归零状态构造。
        if (factionA == nullptr || factionB == nullptr)
        {
            return;
        }

        auto playerA = autochess::core::PlayerStateService::createInitial(
            autochess::core::MapSide::A,
            bundle.gameConfig,
            *factionA);
        auto playerB = autochess::core::PlayerStateService::createInitial(
            autochess::core::MapSide::B,
            bundle.gameConfig,
            *factionB);
        playerA.guardValue = 3;
        playerB.guardValue = 4;
        autochess::core::BattleSummary battle;
        battle.guardDamageToA = 10;
        battle.guardDamageToB = 8;
        autochess::core::RoundSummary round;
        const auto settlement =
            autochess::core::RoundController::settleBattle(
                playerA,
                playerB,
                battle,
                1,
                round);
        const auto result =
            autochess::core::RoundController::determineMatchResult(
                playerA,
                playerB,
                bundle.gameConfig,
                1);
        runner.check(
            settlement.success
                && playerA.guardValue == 0
                && playerB.guardValue == 0
                && round.loserSide == autochess::core::MapSide::A
                && result.outcome == autochess::core::MatchOutcome::Draw,
            "Acceptance matrix batches double zero and keeps raw-damage loser");
    }

    // 此函数验证非法阶段和越权命令不会改变金币、商品或单位列表。
    void checkIllegalCommandAtomicity(
        AcceptanceRunner& runner,
        autochess::core::ConfigBundle bundle)
    {
        autochess::core::Match match(std::move(bundle));
        const bool prepared =
            match.submit({
                autochess::core::MapSide::A,
                autochess::core::SelectMapCommand{"map_01"}}).success
            && match.submit({
                autochess::core::MapSide::A,
                autochess::core::SelectFactionCommand{"training_team"}})
                   .success
            && match.submit({
                autochess::core::MapSide::A,
                autochess::core::SelectAiStrategyCommand{
                    autochess::core::AiStrategyKind::Defensive}}).success
            && match.submit({
                autochess::core::MapSide::B,
                autochess::core::SelectFactionCommand{"assault_team"}})
                   .success;
        runner.check(prepared, "Acceptance matrix reaches preparation");
        // 此分支在选择流程失败时停止非法命令原子性测试。
        if (!prepared)
        {
            return;
        }

        const auto beforePreparationError =
            match.viewFor(autochess::core::MapSide::A);
        const auto earlySkill = match.submit({
            autochess::core::MapSide::A,
            autochess::core::ReleaseSkillCommand{1}});
        const auto sideBStart = match.submit({
            autochess::core::MapSide::B,
            autochess::core::StartCombatCommand{}});
        const auto afterPreparationError =
            match.viewFor(autochess::core::MapSide::A);
        runner.check(
            !earlySkill.success
                && earlySkill.errorCode
                    == autochess::core::CommandErrorCode::InvalidPhase
                && !sideBStart.success
                && sideBStart.errorCode
                    == autochess::core::CommandErrorCode::InvalidActor
                && beforePreparationError.self->gold
                    == afterPreparationError.self->gold
                && shopsEqual(
                    beforePreparationError.selfShop.value(),
                    afterPreparationError.selfShop.value()),
            "Acceptance matrix keeps preparation state after illegal commands");

        const auto purchase = match.submit({
            autochess::core::MapSide::A,
            autochess::core::PurchaseUnitCommand{0}});
        const auto beforeWrongOwner =
            match.viewFor(autochess::core::MapSide::A);
        const auto wrongOwner = match.submit({
            autochess::core::MapSide::B,
            autochess::core::SellUnitCommand{1}});
        const auto afterWrongOwner =
            match.viewFor(autochess::core::MapSide::A);
        runner.check(
            purchase.success
                && !wrongOwner.success
                && wrongOwner.errorCode
                    == autochess::core::CommandErrorCode::WrongOwner
                && beforeWrongOwner.self->gold == afterWrongOwner.self->gold
                && beforeWrongOwner.self->activeUnits.size()
                    == afterWrongOwner.self->activeUnits.size(),
            "Acceptance matrix keeps ownership state after cross-side command");

        const auto start = match.submit({
            autochess::core::MapSide::A,
            autochess::core::StartCombatCommand{}});
        const auto combatView =
            match.viewFor(autochess::core::MapSide::A);
        const auto combatPurchase = match.submit({
            autochess::core::MapSide::A,
            autochess::core::PurchaseUnitCommand{1}});
        const auto missingSkillUnit = match.submit({
            autochess::core::MapSide::A,
            autochess::core::ReleaseSkillCommand{999}});
        const auto afterCombatErrors =
            match.viewFor(autochess::core::MapSide::A);
        runner.check(
            start.success
                && !combatPurchase.success
                && combatPurchase.errorCode
                    == autochess::core::CommandErrorCode::InvalidPhase
                && !missingSkillUnit.success
                && missingSkillUnit.errorCode
                    == autochess::core::CommandErrorCode::UnitNotFound
                && combatView.self->gold == afterCombatErrors.self->gold
                && shopsEqual(
                    combatView.selfShop.value(),
                    afterCombatErrors.selfShop.value()),
            "Acceptance matrix keeps combat state after invalid commands");
    }
}

// 此函数依次执行第六天全部补充验收场景并返回失败数量。
int runMatchAcceptanceTests()
{
    AcceptanceRunner runner;
    autochess::core::ConfigBundle bundle;
    autochess::core::ConfigError loadError;
    const bool loaded = autochess::core::ConfigBundleLoader::load(
        AUTOCHESS_DATA_DIR,
        bundle,
        loadError);
    runner.check(loaded, "Match acceptance loads formal configuration");
    // 此分支在正式配置加载失败时停止全部验收矩阵。
    if (!loaded)
    {
        return runner.failureCount();
    }

    checkRoundIncomeMatrix(runner, bundle);
    checkOutcomeMatrix(runner, bundle);
    checkSimultaneousSettlement(runner, bundle);
    checkIllegalCommandAtomicity(runner, bundle);
    return runner.failureCount();
}
