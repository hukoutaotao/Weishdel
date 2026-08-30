#include "MatchScriptedTests.hpp"

#include "core/config/ConfigBundleLoader.hpp"
#include "core/combat/BattleSimulation.hpp"
#include "core/controllers/IPlayerController.hpp"
#include "core/match/Match.hpp"
#include "core/model/PlayerStateService.hpp"

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

// 此匿名命名空间保存完整对局测试专用的脚本控制器和检查工具。
namespace
{
    // 此测试运行器输出独立脚本测试结果并累计失败数量。
    class ScriptedTestRunner
    {
    public:
        // 此函数按条件输出通过或失败并更新失败计数。
        void check(const bool condition, const std::string& name)
        {
            // 此分支输出通过项目且不增加失败计数。
            if (condition)
            {
                std::cout << "[PASS] " << name << '\n';
                return;
            }

            std::cerr << "[FAIL] " << name << '\n';
            ++failureCount_;
        }

        // 此函数返回当前累计失败数量。
        int failureCount() const noexcept
        {
            return failureCount_;
        }

    private:
        int failureCount_ = 0;
    };

    // 此控制器用最小合法策略驱动选择、购买、部署、开战和技能释放。
    class ScriptedController final : public autochess::core::IPlayerController
    {
    public:
        // 此构造函数保存控制阵营以及测试指定的分队 ID。
        ScriptedController(
            const autochess::core::MapSide side,
            std::string factionId)
            : side_(side),
              factionId_(std::move(factionId))
        {
        }

        // 此函数返回脚本控制器在整个对局中负责的阵营。
        autochess::core::MapSide side() const noexcept override
        {
            return side_;
        }

        // 此函数按当前阶段生成一条确定且合法的最小命令。
        std::optional<autochess::core::GameCommand> nextCommand(
            const autochess::core::ReadOnlyGameView& view) override
        {
            // 此分支让 A 方在初始阶段选择正式训练地图。
            if (side_ == autochess::core::MapSide::A
                && view.phase
                    == autochess::core::MatchPhase::MapSelection)
            {
                return autochess::core::GameCommand{
                    side_,
                    autochess::core::SelectMapCommand{"map_01"}};
            }

            // 此分支让 A 方在分队阶段选择脚本指定分队。
            if (side_ == autochess::core::MapSide::A
                && view.phase
                    == autochess::core::MatchPhase::FactionSelection)
            {
                return autochess::core::GameCommand{
                    side_,
                    autochess::core::SelectFactionCommand{factionId_}};
            }

            // 此分支让 A 方选择路线型 AI 以完成选择流程。
            if (side_ == autochess::core::MapSide::A
                && view.phase == autochess::core::MatchPhase::AiSelection
                && view.aiStrategy
                    == autochess::core::AiStrategyKind::Unknown)
            {
                return autochess::core::GameCommand{
                    side_,
                    autochess::core::SelectAiStrategyCommand{
                        autochess::core::AiStrategyKind::Route}};
            }

            // 此分支让 B 方在策略确定后选择测试指定电脑分队。
            if (side_ == autochess::core::MapSide::B
                && view.phase == autochess::core::MatchPhase::AiSelection
                && view.aiStrategy
                    != autochess::core::AiStrategyKind::Unknown
                && view.factionIdB.empty())
            {
                return autochess::core::GameCommand{
                    side_,
                    autochess::core::SelectFactionCommand{factionId_}};
            }

            // 此分支把准备阶段决策交给只使用己方快照的辅助函数。
            if (view.phase == autochess::core::MatchPhase::Preparation)
            {
                return preparationCommand(view);
            }

            // 此分支把战斗阶段决策交给自动满技力释放辅助函数。
            if (view.phase == autochess::core::MatchPhase::Combat)
            {
                return combatCommand(view);
            }

            return std::nullopt;
        }

    private:
        // 此函数购买首个可负担商品、部署首个备用单位或由 A 方开战。
        std::optional<autochess::core::GameCommand> preparationCommand(
            const autochess::core::ReadOnlyGameView& view) const
        {
            // 此分支在玩家状态尚未可见时不产生准备命令。
            if (!view.self.has_value() || !view.selfShop.has_value())
            {
                return std::nullopt;
            }

            const auto& player = view.self.value();
            const auto& shop = view.selfShop.value();
            // 此分支在没有活动单位时购买首个价格可负担的商品。
            if (player.activeUnits.empty())
            {
                // 此循环按槽位顺序查找首个可以买下的商品。
                for (std::size_t slot = 0; slot < shop.offers.size(); ++slot)
                {
                    // 此分支只为非空且价格不超过当前金币的槽位生成购买命令。
                    if (shop.offers[slot].has_value()
                        && shop.offers[slot]->displayedPrice <= player.gold)
                    {
                        return autochess::core::GameCommand{
                            side_,
                            autochess::core::PurchaseUnitCommand{slot}};
                    }
                }

                return std::nullopt;
            }

            // 此分支在尚未部署时把第一个备用单位放到本方第一条路线起点。
            if (player.deployments.empty() && view.selectedMap.has_value())
            {
                autochess::core::OwnedUnitId reserveUnitId =
                    autochess::core::InvalidOwnedUnitId;
                // 此循环按备用槽顺序选择第一个可部署持久单位。
                for (const auto& slot : player.reserveSlots)
                {
                    // 此分支在找到第一个非空备用槽后停止搜索。
                    if (slot.has_value())
                    {
                        reserveUnitId = slot.value();
                        break;
                    }
                }

                // 此循环查找当前阵营第一条合法部署路线。
                for (const autochess::core::Route& route
                     : view.selectedMap->routes)
                {
                    // 此分支在单位和路线都有效时生成部署命令。
                    if (reserveUnitId != autochess::core::InvalidOwnedUnitId
                        && route.side == side_)
                    {
                        return autochess::core::GameCommand{
                            side_,
                            autochess::core::MoveToDeploymentCommand{
                                reserveUnitId,
                                route.start}};
                    }
                }
            }

            // 此分支在双方都完成部署后让 A 方提前开始战斗。
            if (side_ == autochess::core::MapSide::A
                && !player.deployments.empty()
                && !view.opponent.deployments.empty())
            {
                return autochess::core::GameCommand{
                    side_,
                    autochess::core::StartCombatCommand{}};
            }

            return std::nullopt;
        }

        // 此函数在己方存活单位技力充满且技能未激活时请求释放技能。
        std::optional<autochess::core::GameCommand> combatCommand(
            const autochess::core::ReadOnlyGameView& view) const
        {
            // 此循环按战斗单位稳定顺序查找可以释放技能的己方单位。
            for (const autochess::core::BattleUnit& unit : view.battleUnits)
            {
                // 此分支只为满足全部技能条件的己方存活单位生成命令。
                if (unit.side == side_
                    && unit.state == autochess::core::BattleUnitState::Alive
                    && unit.maxMana > 0.0
                    && unit.currentMana >= unit.maxMana
                    && !unit.activeSkill.active)
                {
                    return autochess::core::GameCommand{
                        side_,
                        autochess::core::ReleaseSkillCommand{unit.id}};
                }
            }

            return std::nullopt;
        }

        autochess::core::MapSide side_ = autochess::core::MapSide::Unknown;
        std::string factionId_;
    };
}

// 此函数使用两个脚本控制器自动完成三回合并验证日志和退出边界。
int runScriptedMatchTests()
{
    ScriptedTestRunner runner;
    static_assert(
        std::is_abstract_v<autochess::core::IPlayerController>,
        "IPlayerController must remain abstract");

    autochess::core::ConfigBundle bundle;
    autochess::core::ConfigError loadError;
    const bool loaded = autochess::core::ConfigBundleLoader::load(
        AUTOCHESS_DATA_DIR,
        bundle,
        loadError);
    runner.check(loaded, "Scripted match loads formal configuration");
    // 此分支在正式配置加载失败时停止完整脚本对局。
    if (!loaded)
    {
        return runner.failureCount();
    }

    // 此代码块验证正式持续技能配置允许施法者在效果期间继续沿路线移动。
    const auto skillAllowsMovement = [&bundle](const std::string& skillId)
    {
        const auto iterator = std::find_if(
            bundle.skills.begin(),
            bundle.skills.end(),
            [&skillId](const autochess::core::SkillDefinition& skill)
            {
                return skill.id == skillId;
            });
        if (iterator == bundle.skills.end() || !iterator->allowMove)
        {
            return false;
        }

        autochess::core::BattleUnit caster;
        caster.id = 1;
        caster.ownedUnitId = 1;
        caster.identity = {"movement_test", 1};
        caster.side = autochess::core::MapSide::A;
        caster.baseStats.maxHealth = 100.0;
        caster.baseStats.physicalDefense = 10.0;
        caster.baseStats.attackSpeed = 1.0;
        caster.baseStats.moveSpeed = 1.0;
        caster.stats = caster.baseStats;
        caster.health = caster.stats.maxHealth;
        caster.skillId = iterator->id;
        caster.currentMana = 10.0;
        caster.maxMana = 10.0;
        caster.position = {0.5, 0.5};
        caster.routePoints = {{0, 0}, {1, 0}, {2, 0}};
        caster.nextRoutePointIndex = 1;

        autochess::core::MapDefinition map;
        map.id = "skill_movement_test";
        map.width = 3;
        map.height = 1;
        map.gridRows = {"..."};

        autochess::core::BattleSimulation simulation(
            {caster}, map, 5, {*iterator});
        if (!simulation.releaseSkill(caster.id)
            || !simulation.units().front().activeSkill.active)
        {
            return false;
        }

        const double positionBefore = simulation.units().front().position.x;
        simulation.step();
        const auto& movedCaster = simulation.units().front();
        return movedCaster.activeSkill.active
            && movedCaster.position.x > positionBefore;
    };
    runner.check(
        skillAllowsMovement("training_strike")
            && skillAllowsMovement("ranger_focus"),
        "Timed buff skills allow movement while active");

    bundle.gameConfig.preparationSeconds = 1;
    bundle.gameConfig.combatTimeoutSeconds = 2;
    autochess::core::Match match(bundle);
    ScriptedController controllerA(
        autochess::core::MapSide::A,
        "training_team");
    ScriptedController controllerB(
        autochess::core::MapSide::B,
        "assault_team");

    int frameCount = 0;
    int commandFailures = 0;
    int loggedRounds = 0;
    constexpr int maximumFrames = 1200;
    // 此循环让两个控制器逐帧读取快照、提交命令并推进核心对局。
    while (match.phase() != autochess::core::MatchPhase::MatchResult
           && frameCount < maximumFrames)
    {
        const auto viewA = match.viewFor(controllerA.side());
        // 此分支提交 A 方本帧生成的至多一条命令。
        if (const auto command = controllerA.nextCommand(viewA))
        {
            // 此分支累计任何不应出现的脚本命令失败。
            if (!match.submit(command.value()).success)
            {
                ++commandFailures;
            }
        }

        const auto viewB = match.viewFor(controllerB.side());
        // 此分支提交 B 方基于最新状态生成的至多一条命令。
        if (const auto command = controllerB.nextCommand(viewB))
        {
            // 此分支累计任何不应出现的脚本命令失败。
            if (!match.submit(command.value()).success)
            {
                ++commandFailures;
            }
        }

        match.step();
        ++frameCount;
        const auto logView = match.viewFor(autochess::core::MapSide::A);
        // 此分支保证每个新完成回合只输出一次稳定摘要。
        if (logView.lastRound.has_value()
            && logView.lastRound->roundNumber > loggedRounds)
        {
            std::cout << logView.lastRound.value() << '\n';
            loggedRounds = logView.lastRound->roundNumber;
        }
    }

    const auto finalView = match.viewFor(autochess::core::MapSide::A);
    // 此分支只在对局确实结束时输出一次最终结果日志。
    if (finalView.phase == autochess::core::MatchPhase::MatchResult)
    {
        std::cout << finalView.result << '\n';
    }

    runner.check(
        commandFailures == 0,
        "Scripted controllers submit only legal commands");
    runner.check(
        frameCount < maximumFrames,
        "Scripted match finishes before the frame guard");
    runner.check(
        finalView.phase == autochess::core::MatchPhase::MatchResult
            && finalView.result.completedRounds == 3,
        "Scripted match completes exactly three rounds");
    runner.check(
        loggedRounds == 3,
        "Scripted match prints one summary for every round");

    return runner.failureCount();
}
