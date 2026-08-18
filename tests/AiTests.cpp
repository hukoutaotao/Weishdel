#include "AiTests.hpp"

#include "core/ai/AiController.hpp"
#include "core/ai/AiDecisionSupport.hpp"
#include "core/ai/DefensiveStrategy.hpp"
#include "core/ai/OffensiveStrategy.hpp"
#include "core/ai/RouteStrategy.hpp"
#include "core/config/ConfigBundleLoader.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace
{
    using namespace autochess::core;

    // 此运行器为独立 AI 测试源统一记录通过项和失败数量。
    class AiTestRunner
    {
    public:
        // 此函数输出单项结果并在失败时累加计数。
        void check(const bool condition, const std::string& name)
        {
            if (condition)
            {
                std::cout << "[PASS] " << name << '\n';
                return;
            }
            std::cerr << "[FAIL] " << name << '\n';
            ++failureCount_;
        }

        // 此函数返回当前测试源累计失败数量。
        int failureCount() const noexcept
        {
            return failureCount_;
        }

    private:
        int failureCount_ = 0;
    };

    // 此函数按稳定 ID 从正式配置中寻找测试地图。
    const MapDefinition* findMap(
        const ConfigBundle& config,
        const std::string& mapId)
    {
        // 此循环返回第一个匹配 ID 的正式地图定义。
        for (const MapDefinition& map : config.maps)
        {
            if (map.id == mapId)
            {
                return &map;
            }
        }
        return nullptr;
    }

    // 此函数构造拥有三个固定商品的准备阶段 B 方快照。
    ReadOnlyGameView makePurchaseView(
        const ConfigBundle& config,
        const std::string& factionId)
    {
        ReadOnlyGameView view;
        view.viewer = MapSide::B;
        view.phase = MatchPhase::Preparation;
        const MapDefinition* map = findMap(config, "map_02");
        // 此分支只在正式地图存在时复制地图，缺失会由测试断言暴露。
        if (map != nullptr)
        {
            view.selectedMap = *map;
            view.selectedMapId = map->id;
        }

        PlayerState player;
        player.side = MapSide::B;
        player.factionId = factionId;
        player.gold = 100;
        player.guardValue = 100;
        player.reserveSlots.resize(4);
        view.self = player;

        ShopState shop;
        // 此代码块让三种策略面对完全相同且都可负担的候选商品。
        shop.offers = {
            ShopOffer{"duelist", 1},
            ShopOffer{"ranger", 1},
            ShopOffer{"medic", 1}};
        view.selfShop = shop;
        return view;
    }

    // 此函数从购买命令反查测试快照中的单位 ID。
    std::string purchasedUnitId(
        const std::optional<GameCommand>& command,
        const ReadOnlyGameView& view)
    {
        // 此分支拒绝空命令、错误命令类型或缺失商店。
        if (!command.has_value()
            || !std::holds_alternative<PurchaseUnitCommand>(
                command->payload)
            || !view.selfShop.has_value())
        {
            return {};
        }
        const std::size_t slot =
            std::get<PurchaseUnitCommand>(command->payload).shopSlot;
        // 此分支拒绝越界槽位和空商品槽位。
        if (slot >= view.selfShop->offers.size()
            || !view.selfShop->offers[slot].has_value())
        {
            return {};
        }
        return view.selfShop->offers[slot]->unitId;
    }

    // 此函数构造己方路线长度与对应敌方威胁长度相反的测试地图。
    MapDefinition makeRouteChoiceMap()
    {
        MapDefinition map;
        map.id = "ai_route_choice";
        // 此代码块让进攻/路线策略偏好 beta，而防守策略偏好 alpha。
        map.routes = {
            Route{"a_alpha", MapSide::A, {1, 1}, {{1, 1}, {2, 1}}},
            Route{
                "a_beta",
                MapSide::A,
                {1, 2},
                {{1, 2}, {2, 2}, {3, 2}, {4, 2}, {5, 2}, {6, 2}}},
            Route{
                "b_alpha",
                MapSide::B,
                {8, 1},
                {{8, 1}, {7, 1}, {6, 1}, {5, 1}, {4, 1}, {3, 1}}},
            Route{"b_beta", MapSide::B, {8, 2}, {{8, 2}, {7, 2}}}};
        return map;
    }

    // 此函数构造带一个备用单位的部署决策快照。
    ReadOnlyGameView makeDeploymentView(const std::string& factionId)
    {
        ReadOnlyGameView view;
        view.viewer = MapSide::B;
        view.phase = MatchPhase::Preparation;
        view.selectedMap = makeRouteChoiceMap();

        PlayerState player;
        player.side = MapSide::B;
        player.factionId = factionId;
        player.gold = 100;
        player.guardValue = 100;
        player.activeUnits.push_back(
            OwnedUnit{7, {"training_guard", 1}, MapSide::B});
        player.reserveSlots.resize(4);
        player.reserveSlots[0] = 7;
        view.self = player;
        view.selfShop = ShopState{};
        return view;
    }

    // 此函数从部署命令中读取目标格，类型不匹配时返回空值。
    std::optional<GridPosition> deploymentTarget(
        const std::optional<GameCommand>& command)
    {
        // 此分支只接受正式部署命令。
        if (!command.has_value()
            || !std::holds_alternative<MoveToDeploymentCommand>(
                command->payload))
        {
            return std::nullopt;
        }
        return std::get<MoveToDeploymentCommand>(command->payload).target;
    }

    // 此函数构造具备完整技力和基础战斗属性的测试单位。
    BattleUnit makeBattleUnit(
        const BattleUnitId id,
        const MapSide side,
        const std::string& skillId,
        const double attackPower,
        const BattlePosition position)
    {
        BattleUnit unit;
        unit.id = id;
        unit.ownedUnitId = id;
        unit.identity = {"test_unit", 1};
        unit.side = side;
        unit.skillId = skillId;
        unit.maxMana = 10.0;
        unit.currentMana = 10.0;
        unit.baseStats.maxHealth = 100.0;
        unit.baseStats.attackPower = attackPower;
        unit.stats = unit.baseStats;
        unit.health = 100.0;
        unit.position = position;
        unit.state = BattleUnitState::Alive;
        return unit;
    }

    // 此函数构造能同时触发伤害、攻速增益和治疗技能的战斗快照。
    ReadOnlyGameView makeCombatView()
    {
        ReadOnlyGameView view;
        view.viewer = MapSide::B;
        view.phase = MatchPhase::Combat;

        BattleUnit duelist = makeBattleUnit(
            10,
            MapSide::B,
            "duelist_slash",
            100.0,
            {0.0, 0.0});
        BattleUnit ranger = makeBattleUnit(
            20,
            MapSide::B,
            "ranger_focus",
            10.0,
            {0.0, 0.0});
        BattleUnit medic = makeBattleUnit(
            30,
            MapSide::B,
            "field_mend",
            1.0,
            {0.0, 0.0});
        BattleUnit enemy = makeBattleUnit(
            40,
            MapSide::A,
            "ranger_focus",
            1.0,
            {1.0, 0.0});
        // 此代码块让医师拥有合法受伤友军，同时让决斗者拥有近距离敌人。
        ranger.health = 50.0;
        view.battleUnits = {duelist, ranger, medic, enemy};
        return view;
    }

    // 此函数从技能命令中读取释放者 ID，类型不匹配时返回空值。
    std::optional<BattleUnitId> releasedUnitId(
        const std::optional<GameCommand>& command)
    {
        // 此分支只接受正式技能释放命令。
        if (!command.has_value()
            || !std::holds_alternative<ReleaseSkillCommand>(command->payload))
        {
            return std::nullopt;
        }
        return std::get<ReleaseSkillCommand>(command->payload).battleUnitId;
    }

    // 此函数从选分队命令中读取 ID，类型不匹配时返回空字符串。
    std::string selectedFactionId(
        const std::optional<GameCommand>& command)
    {
        // 此分支只接受正式分队选择命令。
        if (!command.has_value()
            || !std::holds_alternative<SelectFactionCommand>(
                command->payload))
        {
            return {};
        }
        return std::get<SelectFactionCommand>(command->payload).factionId;
    }

    // 此函数构造包含正式分队候选的 AI 选择快照。
    ReadOnlyGameView makeAiSelectionView(
        const ConfigBundle& config,
        const AiStrategyKind strategy)
    {
        ReadOnlyGameView view;
        view.viewer = MapSide::B;
        view.phase = MatchPhase::AiSelection;
        view.aiStrategy = strategy;
        // 此循环把正式配置转换为控制器只能读取的选择项。
        for (const FactionDefinition& faction : config.factions)
        {
            view.factions.push_back({faction.id, faction.name});
        }
        return view;
    }
}

// 此函数依次验证三种策略的可识别差异和统一控制器边界。
int runAiStrategyTests()
{
    AiTestRunner runner;
    ConfigBundle config;
    ConfigError error;
    const bool loaded = ConfigBundleLoader::load(
        AUTOCHESS_DATA_DIR,
        config,
        error);
    runner.check(loaded, "AI tests load formal configuration");
    // 此分支在正式配置缺失时停止所有依赖配置的断言。
    if (!loaded)
    {
        return runner.failureCount();
    }

    OffensiveStrategy offensive;
    DefensiveStrategy defensive;
    RouteStrategy route;
    // 此代码块验证策略枚举与默认分队的冻结映射。
    runner.check(
        offensive.kind() == AiStrategyKind::Offensive
            && std::string(offensive.defaultFactionId()) == "assault_team"
            && defensive.kind() == AiStrategyKind::Defensive
            && std::string(defensive.defaultFactionId()) == "training_team"
            && route.kind() == AiStrategyKind::Route
            && std::string(route.defaultFactionId()) == "route_team",
        "AI strategies expose stable kinds and default factions");

    const ReadOnlyGameView emptyView;
    AiController emptyController(config);
    // 此代码块验证空快照不会产生购买、部署、技能或选择命令。
    runner.check(
        !emptyController.nextCommand(emptyView).has_value()
            && !offensive.choosePreparationCommand(
                    MapSide::B,
                    emptyView,
                    config)
                    .has_value()
            && !defensive.chooseCombatCommand(
                    MapSide::B,
                    emptyView,
                    config)
                    .has_value()
            && !route.choosePreparationCommand(
                    MapSide::B,
                    emptyView,
                    config)
                    .has_value(),
        "AI returns no command for an empty snapshot");

    AiController defaultOffensive(config);
    AiController defaultDefensive(config);
    AiController defaultRoute(config);
    const auto offensiveFaction = defaultOffensive.nextCommand(
        makeAiSelectionView(config, AiStrategyKind::Offensive));
    const auto defensiveFaction = defaultDefensive.nextCommand(
        makeAiSelectionView(config, AiStrategyKind::Defensive));
    const auto routeFaction = defaultRoute.nextCommand(
        makeAiSelectionView(config, AiStrategyKind::Route));
    // 此代码块验证控制器按策略提交三个默认分队且命令归属 B 方。
    runner.check(
        selectedFactionId(offensiveFaction) == "assault_team"
            && selectedFactionId(defensiveFaction) == "training_team"
            && selectedFactionId(routeFaction) == "route_team"
            && offensiveFaction->actor == MapSide::B
            && defensiveFaction->actor == MapSide::B
            && routeFaction->actor == MapSide::B,
        "AI controller selects each default faction as side B");

    AiController forcedFaction(config, MapSide::B, "route_team");
    AiController invalidForcedFaction(config, MapSide::B, "missing_team");
    AiController invalidSide(config, MapSide::Unknown);
    const ReadOnlyGameView offensiveSelection = makeAiSelectionView(
        config,
        AiStrategyKind::Offensive);
    // 此代码块验证集成测试覆盖值有效，并拒绝不存在分队和未知阵营。
    runner.check(
        selectedFactionId(forcedFaction.nextCommand(offensiveSelection))
                == "route_team"
            && !invalidForcedFaction.nextCommand(offensiveSelection)
                    .has_value()
            && !invalidSide.nextCommand(offensiveSelection).has_value(),
        "AI controller validates forced faction and controller side");

    ReadOnlyGameView offensivePurchase = makePurchaseView(
        config,
        "assault_team");
    ReadOnlyGameView defensivePurchase = makePurchaseView(
        config,
        "training_team");
    ReadOnlyGameView routePurchase = makePurchaseView(
        config,
        "route_team");
    const auto offensiveBuy = offensive.choosePreparationCommand(
        MapSide::B,
        offensivePurchase,
        config);
    const auto defensiveBuy = defensive.choosePreparationCommand(
        MapSide::B,
        defensivePurchase,
        config);
    const auto routeBuy = route.choosePreparationCommand(
        MapSide::B,
        routePurchase,
        config);
    // 此代码块验证同一商品集合分别产生进攻、防守和路线型购买结果。
    runner.check(
        purchasedUnitId(offensiveBuy, offensivePurchase) == "duelist"
            && purchasedUnitId(defensiveBuy, defensivePurchase) == "medic"
            && purchasedUnitId(routeBuy, routePurchase) == "ranger"
            && offensiveBuy->actor == MapSide::B
            && defensiveBuy->actor == MapSide::B
            && routeBuy->actor == MapSide::B,
        "AI strategies choose distinct purchases from the same offers");

    const auto offensiveDeploy = offensive.choosePreparationCommand(
        MapSide::B,
        makeDeploymentView("assault_team"),
        config);
    const auto defensiveDeploy = defensive.choosePreparationCommand(
        MapSide::B,
        makeDeploymentView("training_team"),
        config);
    const auto routeDeploy = route.choosePreparationCommand(
        MapSide::B,
        makeDeploymentView("route_team"),
        config);
    // 此代码块验证防守策略追踪最短敌路，另外两种策略选择最短己路。
    runner.check(
        deploymentTarget(offensiveDeploy) == GridPosition{8, 2}
            && deploymentTarget(defensiveDeploy) == GridPosition{8, 1}
            && deploymentTarget(routeDeploy) == GridPosition{8, 2},
        "AI strategies apply their route priorities deterministically");

    const ReadOnlyGameView combatView = makeCombatView();
    const auto offensiveSkill = offensive.chooseCombatCommand(
        MapSide::B,
        combatView,
        config);
    const auto defensiveSkill = defensive.chooseCombatCommand(
        MapSide::B,
        combatView,
        config);
    const auto routeSkill = route.chooseCombatCommand(
        MapSide::B,
        combatView,
        config);
    // 此代码块验证进攻、防守和路线策略分别偏好伤害、治疗和攻速技能。
    runner.check(
        releasedUnitId(offensiveSkill) == BattleUnitId{10}
            && releasedUnitId(defensiveSkill) == BattleUnitId{30}
            && releasedUnitId(routeSkill) == BattleUnitId{20}
            && offensiveSkill->actor == MapSide::B
            && defensiveSkill->actor == MapSide::B
            && routeSkill->actor == MapSide::B,
        "AI strategies choose distinct ready skills");

    ReadOnlyGameView skillConditionView;
    skillConditionView.viewer = MapSide::B;
    BattleUnit caster = makeBattleUnit(
        50,
        MapSide::B,
        "duelist_slash",
        20.0,
        {0.0, 0.0});
    skillConditionView.battleUnits = {caster};
    const bool rejectedWithoutTarget = !AiDecisionSupport::skillCanRelease(
        skillConditionView.battleUnits.front(),
        skillConditionView,
        config);
    skillConditionView.battleUnits.push_back(makeBattleUnit(
        51,
        MapSide::A,
        "ranger_focus",
        1.0,
        {1.0, 0.0}));
    const bool acceptedWithTarget = AiDecisionSupport::skillCanRelease(
        skillConditionView.battleUnits.front(),
        skillConditionView,
        config);
    skillConditionView.battleUnits.front().currentMana = 9.0;
    const bool rejectedWithoutMana = !AiDecisionSupport::skillCanRelease(
        skillConditionView.battleUnits.front(),
        skillConditionView,
        config);
    // 此代码块验证伤害技能同时需要合法目标、满技力和观察阵营归属。
    runner.check(
        rejectedWithoutTarget
            && acceptedWithTarget
            && rejectedWithoutMana
            && !AiDecisionSupport::skillCanRelease(
                skillConditionView.battleUnits.back(),
                skillConditionView,
                config),
        "AI skill checks enforce target mana and own-side conditions");

    ReadOnlyGameView opponentOnly;
    opponentOnly.viewer = MapSide::B;
    opponentOnly.phase = MatchPhase::Combat;
    opponentOnly.battleUnits.push_back(makeBattleUnit(
        60,
        MapSide::A,
        "ranger_focus",
        100.0,
        {0.0, 0.0}));
    // 此代码块验证任何策略都不会为对手满技力单位提交命令。
    runner.check(
        !offensive.chooseCombatCommand(MapSide::B, opponentOnly, config)
             .has_value()
            && !defensive.chooseCombatCommand(MapSide::B, opponentOnly, config)
                    .has_value()
            && !route.chooseCombatCommand(MapSide::B, opponentOnly, config)
                    .has_value(),
        "AI strategies never release an opponent skill");

    offensivePurchase.aiStrategy = AiStrategyKind::Offensive;
    AiController preparationController(config, MapSide::B);
    const auto ownedPreparationCommand =
        preparationController.nextCommand(offensivePurchase);
    // 此代码块验证统一控制器转发的准备命令保持自身阵营且不是开战命令。
    runner.check(
        ownedPreparationCommand.has_value()
            && ownedPreparationCommand->actor == MapSide::B
            && !std::holds_alternative<StartCombatCommand>(
                ownedPreparationCommand->payload),
        "AI controller forwards only an owned non-start command");

    return runner.failureCount();
}
