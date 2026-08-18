#include "core/Core.hpp"
#include "core/combat/BattleSimulation.hpp"
#include "core/combat/BattleSetupService.hpp"
#include "core/combat/BattleStatResolver.hpp"
#include "core/combat/BattleTypes.hpp"
#include "core/combat/CombatRules.hpp"
#include "core/combat/TargetSelector.hpp"
#include "core/config/ConfigBundleLoader.hpp"
#include "core/config/ConfigError.hpp"
#include "core/config/DefinitionConfigLoader.hpp"
#include "core/config/GameConfigLoader.hpp"
#include "core/config/MapConfigLoader.hpp"
#include "core/config/ConfigParser.hpp"
#include "core/economy/DeploymentService.hpp"
#include "core/economy/EconomyTypes.hpp"
#include "core/economy/MergeService.hpp"
#include "core/economy/PriceRules.hpp"
#include "core/economy/RosterService.hpp"
#include "core/economy/ShopService.hpp"
#include "core/map/MapTypes.hpp"
#include "core/model/Definitions.hpp"
#include "core/model/PlayerStateService.hpp"
#include "core/model/PlayerTypes.hpp"
#include "core/skills/Skill.hpp"
#include "core/skills/SkillSystem.hpp"
#include "core/units/Unit.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
    // 此函数验证核心数据类型可以被构造并保存基础值。
    bool runCoreTypesSmokeTest()
    {
        autochess::core::GameConfig config;
        config.maxRounds = 3;
        config.randomSeed = 20260814;

        autochess::core::UnitDefinition unit;
        unit.id = "training_guard";
        unit.levelMultipliers = {1.0, 1.5, 2.0};

        const autochess::core::GridPosition firstPosition{1, 3};
        const autochess::core::GridPosition samePosition{1, 3};

        autochess::core::MapDefinition map;
        map.id = "training_map";
        map.width = 7;
        map.height = 5;
        map.gridRows = {
            "#######",
            "#X...B#",
            "#.....#",
            "#A...Y#",
            "#######"};

        autochess::core::ConfigError error;
        error.category =
            autochess::core::ConfigErrorCategory::MissingField;
        error.sourcePath = "data/game.cfg";
        error.line = 2;
        error.message = "缺少字段";

        return config.maxRounds == 3
            && config.randomSeed == 20260814
            && unit.id == "training_guard"
            && unit.levelMultipliers.size() == 3
            && firstPosition == samePosition
            && map.gridRows.size() == 5
            && error.category
                == autochess::core::ConfigErrorCategory::MissingField
            && error.line == 2;
    }

    // 此函数验证持久单位、备用区和部署映射可以表达玩家的基础状态。
    bool runPlayerTypesSmokeTest()
    {
        const autochess::core::UnitIdentity firstIdentity{
            "training_guard", 1};
        const autochess::core::UnitIdentity sameIdentity{
            "training_guard", 1};
        const autochess::core::UnitIdentity differentType{
            "training_archer", 1};
        const autochess::core::UnitIdentity differentLevel{
            "training_guard", 2};

        const autochess::core::OwnedUnit reserveUnit{
            1, firstIdentity, autochess::core::MapSide::A};
        const autochess::core::OwnedUnit firstDeployedUnit{
            2, firstIdentity, autochess::core::MapSide::A};
        const autochess::core::OwnedUnit secondDeployedUnit{
            3, firstIdentity, autochess::core::MapSide::A};
        const autochess::core::OwnedUnit deadUnit{
            4, differentLevel, autochess::core::MapSide::A};

        autochess::core::PlayerState player;
        player.side = autochess::core::MapSide::A;
        player.factionId = "training_team";
        player.gold = 10;
        player.guardValue = 100;
        player.activeUnits = {
            reserveUnit, firstDeployedUnit, secondDeployedUnit};
        player.deadUnits.push_back(deadUnit);
        player.reserveSlots.resize(8);
        player.reserveSlots[0] = reserveUnit.id;

        const auto firstDeployment = player.deployments.emplace(
            autochess::core::GridPosition{1, 2}, firstDeployedUnit.id);
        const auto secondDeployment = player.deployments.emplace(
            autochess::core::GridPosition{2, 1}, secondDeployedUnit.id);
        const auto duplicateDeployment = player.deployments.emplace(
            autochess::core::GridPosition{1, 2}, reserveUnit.id);

        return firstIdentity == sameIdentity
            && !(firstIdentity == differentType)
            && !(firstIdentity == differentLevel)
            && reserveUnit.id == 1
            && reserveUnit.identity == firstIdentity
            && reserveUnit.ownerSide == autochess::core::MapSide::A
            && player.side == autochess::core::MapSide::A
            && player.factionId == "training_team"
            && player.gold == 10
            && player.guardValue == 100
            && player.activeUnits.size() == 3
            && player.deadUnits.size() == 1
            && player.reserveSlots.size() == 8
            && player.reserveSlots[0].has_value()
            && player.reserveSlots[0].value() == reserveUnit.id
            && !player.reserveSlots[1].has_value()
            && firstDeployment.second
            && secondDeployment.second
            && !duplicateDeployment.second
            && player.deployments.size() == 2
            && player.deployments.at({1, 2}) == firstDeployedUnit.id
            && player.deployments.at({2, 1}) == secondDeployedUnit.id;
    }

    // 此函数验证商店槽位和命令结果能够保存准备阶段的公共数据。
    bool runEconomyTypesSmokeTest()
    {
        autochess::core::ShopState shop;
        shop.offers.resize(6);
        shop.offers[0] = autochess::core::ShopOffer{
            "training_guard", 3};

        const autochess::core::CommandResult successResult{
            true,
            autochess::core::CommandErrorCode::None,
            "操作成功"};
        const autochess::core::CommandResult failureResult{
            false,
            autochess::core::CommandErrorCode::InsufficientGold,
            "金币不足"};

        return shop.offers.size() == 6
            && shop.offers[0].has_value()
            && shop.offers[0]->unitId == "training_guard"
            && shop.offers[0]->displayedPrice == 3
            && !shop.offers[1].has_value()
            && successResult.success
            && successResult.errorCode
                == autochess::core::CommandErrorCode::None
            && !failureResult.success
            && failureResult.errorCode
                == autochess::core::CommandErrorCode::InsufficientGold
            && !failureResult.message.empty();
    }

    // 此测试运行器统一输出测试结果并累计失败数量。
    class TestRunner
    {
    public:
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

        int failureCount() const noexcept
        {
            return failureCount_;
        }

    private:
        int failureCount_ = 0;
    };

    bool shopsAreEqual(
        const autochess::core::ShopState& left,
        const autochess::core::ShopState& right)
    {
        if (left.offers.size() != right.offers.size())
        {
            return false;
        }

        for (std::size_t slot = 0; slot < left.offers.size(); ++slot)
        {
            if (left.offers[slot].has_value()
                != right.offers[slot].has_value())
            {
                return false;
            }

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

    bool ownedUnitListsAreEqual(
        const std::vector<autochess::core::OwnedUnit>& left,
        const std::vector<autochess::core::OwnedUnit>& right)
    {
        if (left.size() != right.size())
        {
            return false;
        }

        for (std::size_t index = 0; index < left.size(); ++index)
        {
            if (left[index].id != right[index].id
                || !(left[index].identity == right[index].identity)
                || left[index].ownerSide != right[index].ownerSide)
            {
                return false;
            }
        }

        return true;
    }

    bool playersAreEqual(
        const autochess::core::PlayerState& left,
        const autochess::core::PlayerState& right)
    {
        return left.side == right.side
            && left.factionId == right.factionId
            && left.gold == right.gold
            && left.guardValue == right.guardValue
            && ownedUnitListsAreEqual(left.activeUnits, right.activeUnits)
            && ownedUnitListsAreEqual(left.deadUnits, right.deadUnits)
            && left.reserveSlots == right.reserveSlots
            && left.deployments == right.deployments;
    }

    int runPriceRulesTests()
    {
        TestRunner runner;

        autochess::core::UnitDefinition unit;
        unit.id = "price_unit";
        unit.price = 3;

        autochess::core::FactionDefinition faction;
        faction.id = "price_team";
        faction.priceMultiplier = 1.0;

        const std::vector<autochess::core::FactionModifierDefinition>
            noModifiers;
        const auto basePrice =
            autochess::core::PriceRules::calculateLevelOnePrice(
                unit, faction, noModifiers);
        runner.check(
            basePrice.success
                && basePrice.amount == 3
                && basePrice.errorCode
                    == autochess::core::PriceErrorCode::None
                && basePrice.message.empty(),
            "PriceRules calculates the unmodified base price");

        faction.priceMultiplier = 1.5;
        const auto factionPrice =
            autochess::core::PriceRules::calculateLevelOnePrice(
                unit, faction, noModifiers);
        runner.check(
            factionPrice.success && factionPrice.amount == 5,
            "PriceRules applies the faction price multiplier");

        faction.priceMultiplier = 1.0;
        const std::vector<autochess::core::FactionModifierDefinition>
            matchingModifiers{
                {"price_add", "price_team", "price_unit",
                    autochess::core::FactionAttribute::Price,
                    autochess::core::FactionOperation::Add, 1.0},
                {"price_multiply", "price_team", "price_unit",
                    autochess::core::FactionAttribute::Price,
                    autochess::core::FactionOperation::Multiply, 1.2}};
        const auto adjustedPrice =
            autochess::core::PriceRules::calculateLevelOnePrice(
                unit, faction, matchingModifiers);
        runner.check(
            adjustedPrice.success && adjustedPrice.amount == 5,
            "PriceRules applies add and multiply price modifiers");

        const std::vector<autochess::core::FactionModifierDefinition>
            unrelatedModifiers{
                {"other_faction", "other_team", "price_unit",
                    autochess::core::FactionAttribute::Price,
                    autochess::core::FactionOperation::Add, 100.0},
                {"other_unit", "price_team", "other_unit",
                    autochess::core::FactionAttribute::Price,
                    autochess::core::FactionOperation::Multiply, 2.0},
                {"other_attribute", "price_team", "price_unit",
                    autochess::core::FactionAttribute::AttackPower,
                    autochess::core::FactionOperation::Add, 100.0}};
        const auto filteredPrice =
            autochess::core::PriceRules::calculateLevelOnePrice(
                unit, faction, unrelatedModifiers);
        runner.check(
            filteredPrice.success && filteredPrice.amount == 3,
            "PriceRules ignores unrelated modifiers");

        const auto levelOnePrice =
            autochess::core::PriceRules::calculateLevelOnePrice(
                unit, faction, noModifiers);
        const auto levelTwoOwnedUnit = autochess::core::OwnedUnit{
            2,
            {unit.id, 2},
            autochess::core::MapSide::A};
        const auto levelThreeOwnedUnit = autochess::core::OwnedUnit{
            3,
            {unit.id, 3},
            autochess::core::MapSide::A};
        runner.check(
            levelOnePrice.success
                && levelTwoOwnedUnit.identity.unitId == unit.id
                && levelThreeOwnedUnit.identity.unitId == unit.id
                && levelOnePrice.amount == 3,
            "PriceRules keeps the price independent of owned unit level");

        const auto mergeRefund =
            autochess::core::PriceRules::calculateRatioAmount(3, 0.40);
        const auto sellRefund =
            autochess::core::PriceRules::calculateRatioAmount(3, 0.75);
        const auto reviveCost =
            autochess::core::PriceRules::calculateRatioAmount(3, 0.50);
        runner.check(
            mergeRefund.success && mergeRefund.amount == 1
                && sellRefund.success && sellRefund.amount == 2
                && reviveCost.success && reviveCost.amount == 2,
            "PriceRules calculates merge, sell, and revive amounts");

        const auto halfAmount =
            autochess::core::PriceRules::calculateRatioAmount(3, 0.5);
        runner.check(
            halfAmount.success && halfAmount.amount == 2,
            "PriceRules rounds an exact half upward");

        autochess::core::ConfigBundle formalBundle;
        autochess::core::ConfigError formalLoadError;
        const bool formalLoaded =
            autochess::core::ConfigBundleLoader::load(
                AUTOCHESS_DATA_DIR,
                formalBundle,
                formalLoadError);
        bool formalAmountsAreCorrect = false;
        if (formalLoaded
            && formalBundle.units.size() == 5
            && formalBundle.factions.size() == 3
            && formalBundle.units.front().id == "training_guard"
            && formalBundle.factions.front().id == "training_team")
        {
            const auto formalPrice =
                autochess::core::PriceRules::calculateLevelOnePrice(
                    formalBundle.units.front(),
                    formalBundle.factions.front(),
                    formalBundle.factionModifiers);
            const auto formalMergeRefund =
                autochess::core::PriceRules::calculateRatioAmount(
                    formalPrice.amount,
                    formalBundle.gameConfig.mergeRefundRatio);
            const auto formalSellRefund =
                autochess::core::PriceRules::calculateRatioAmount(
                    formalPrice.amount,
                    formalBundle.gameConfig.sellRatio);
            const auto formalReviveCost =
                autochess::core::PriceRules::calculateRatioAmount(
                    formalPrice.amount,
                    formalBundle.gameConfig.reviveRatio);
            formalAmountsAreCorrect = formalPrice.success
                && formalPrice.amount == 3
                && formalMergeRefund.success
                && formalMergeRefund.amount == 1
                && formalSellRefund.success
                && formalSellRefund.amount == 2
                && formalReviveCost.success
                && formalReviveCost.amount == 2;
        }
        runner.check(
            formalAmountsAreCorrect,
            "PriceRules uses the formal price and economy ratios");

        auto invalidUnit = unit;
        invalidUnit.price = 0;
        const auto invalidBasePrice =
            autochess::core::PriceRules::calculateLevelOnePrice(
                invalidUnit, faction, noModifiers);
        runner.check(
            !invalidBasePrice.success
                && invalidBasePrice.errorCode
                    == autochess::core::PriceErrorCode::InvalidBasePrice
                && !invalidBasePrice.message.empty(),
            "PriceRules rejects a non-positive base price");

        const std::vector<autochess::core::FactionModifierDefinition>
            invalidModifiers{
                {"invalid_adjustment", "price_team", "price_unit",
                    autochess::core::FactionAttribute::Price,
                    autochess::core::FactionOperation::Add, -3.0}};
        const auto invalidAdjustedPrice =
            autochess::core::PriceRules::calculateLevelOnePrice(
                unit, faction, invalidModifiers);
        runner.check(
            !invalidAdjustedPrice.success
                && invalidAdjustedPrice.errorCode
                    == autochess::core::PriceErrorCode::InvalidAdjustedPrice
                && !invalidAdjustedPrice.message.empty(),
            "PriceRules rejects a non-positive adjusted price");

        const auto invalidRatio =
            autochess::core::PriceRules::calculateRatioAmount(3, 1.01);
        runner.check(
            !invalidRatio.success
                && invalidRatio.errorCode
                    == autochess::core::PriceErrorCode::InvalidRatio
                && !invalidRatio.message.empty(),
            "PriceRules rejects an out-of-range ratio");

        auto unchangedUnit = unit;
        auto unchangedFaction = faction;
        auto unchangedModifiers = invalidModifiers;
        const auto unchangedResult =
            autochess::core::PriceRules::calculateLevelOnePrice(
                unchangedUnit,
                unchangedFaction,
                unchangedModifiers);
        runner.check(
            !unchangedResult.success
                && unchangedUnit.id == unit.id
                && unchangedUnit.price == unit.price
                && unchangedFaction.id == faction.id
                && unchangedFaction.priceMultiplier
                    == faction.priceMultiplier
                && unchangedModifiers.size() == invalidModifiers.size()
                && unchangedModifiers[0].value
                    == invalidModifiers[0].value,
            "PriceRules failure does not modify its inputs");

        return runner.failureCount();
    }

    int runShopServiceTests()
    {
        TestRunner runner;

        autochess::core::GameConfig gameConfig;
        gameConfig.startingGold = 10;
        gameConfig.shopSlots = 6;
        gameConfig.shopRefreshCost = 2;

        autochess::core::FactionDefinition faction;
        faction.id = "shop_team";
        faction.name = "商店测试分队";
        faction.initialGuard = 100;
        faction.priceMultiplier = 1.0;

        autochess::core::PlayerState player;
        player.side = autochess::core::MapSide::A;
        player.factionId = faction.id;
        player.gold = gameConfig.startingGold;
        player.guardValue = faction.initialGuard;

        std::vector<autochess::core::UnitDefinition> units(3);
        units[0].id = "cheap_unit";
        units[0].price = 2;
        units[1].id = "normal_unit";
        units[1].price = 3;
        units[2].id = "expensive_unit";
        units[2].price = 5;
        const std::vector<autochess::core::FactionModifierDefinition>
            noModifiers;

        autochess::core::ShopState firstShop;
        firstShop.offers.emplace_back(
            autochess::core::ShopOffer{"old_offer", 99});
        autochess::core::ShopState secondShop;
        std::mt19937 firstEngine(20260814u);
        std::mt19937 secondEngine(20260814u);
        const auto firstRebuild = autochess::core::ShopService::rebuild(
            player,
            firstShop,
            gameConfig,
            units,
            faction,
            noModifiers,
            firstEngine);
        const auto secondRebuild = autochess::core::ShopService::rebuild(
            player,
            secondShop,
            gameConfig,
            units,
            faction,
            noModifiers,
            secondEngine);
        runner.check(
            firstRebuild.success
                && secondRebuild.success
                && firstRebuild.errorCode
                    == autochess::core::CommandErrorCode::None
                && !firstRebuild.message.empty()
                && shopsAreEqual(firstShop, secondShop)
                && firstEngine == secondEngine,
            "ShopService rebuilds deterministically with the same seed");

        bool allSlotsContainOffers = firstShop.offers.size()
            == static_cast<std::size_t>(gameConfig.shopSlots);
        bool everyDisplayedPriceIsCorrect = allSlotsContainOffers;
        bool oldOfferWasReplaced = allSlotsContainOffers;
        for (const auto& offer : firstShop.offers)
        {
            if (!offer.has_value())
            {
                allSlotsContainOffers = false;
                everyDisplayedPriceIsCorrect = false;
                oldOfferWasReplaced = false;
                continue;
            }

            int expectedPrice = 0;
            if (offer->unitId == "cheap_unit")
            {
                expectedPrice = 2;
            }
            else if (offer->unitId == "normal_unit")
            {
                expectedPrice = 3;
            }
            else if (offer->unitId == "expensive_unit")
            {
                expectedPrice = 5;
            }

            everyDisplayedPriceIsCorrect = everyDisplayedPriceIsCorrect
                && offer->displayedPrice == expectedPrice
                && expectedPrice > 0;
            oldOfferWasReplaced = oldOfferWasReplaced
                && offer->unitId != "old_offer";
        }
        runner.check(
            allSlotsContainOffers,
            "ShopService uses the configured slot count and fills every slot");
        runner.check(
            everyDisplayedPriceIsCorrect,
            "ShopService stores each unit's calculated level-one price");
        runner.check(
            oldOfferWasReplaced,
            "ShopService rebuild replaces every previous offer");

        const std::vector<autochess::core::UnitDefinition> singleUnitPool{
            units.front()};
        autochess::core::ShopState repeatedShop;
        std::mt19937 repeatedEngine(77u);
        const auto repeatedResult = autochess::core::ShopService::rebuild(
            player,
            repeatedShop,
            gameConfig,
            singleUnitPool,
            faction,
            noModifiers,
            repeatedEngine);
        bool everyOfferIsRepeated = repeatedResult.success
            && repeatedShop.offers.size()
                == static_cast<std::size_t>(gameConfig.shopSlots);
        for (const auto& offer : repeatedShop.offers)
        {
            everyOfferIsRepeated = everyOfferIsRepeated
                && offer.has_value()
                && offer->unitId == "cheap_unit"
                && offer->displayedPrice == 2;
        }
        runner.check(
            everyOfferIsRepeated,
            "ShopService samples an unlimited unit pool with replacement");

        autochess::core::PlayerState refreshedPlayer = player;
        autochess::core::ShopState refreshedShop;
        refreshedShop.offers.emplace_back(
            autochess::core::ShopOffer{"old_offer", 99});
        std::mt19937 refreshEngine(123u);
        const auto refreshResult = autochess::core::ShopService::refresh(
            refreshedPlayer,
            refreshedShop,
            gameConfig,
            units,
            faction,
            noModifiers,
            refreshEngine);
        runner.check(
            refreshResult.success
                && refreshResult.errorCode
                    == autochess::core::CommandErrorCode::None
                && !refreshResult.message.empty()
                && refreshedPlayer.gold == 8
                && refreshedShop.offers.size() == 6
                && refreshedShop.offers.front().has_value()
                && refreshedShop.offers.front()->unitId != "old_offer",
            "ShopService refresh replaces offers and deducts the configured cost");

        autochess::core::PlayerState poorPlayer = player;
        poorPlayer.gold = 1;
        autochess::core::ShopState poorShop;
        poorShop.offers.emplace_back(
            autochess::core::ShopOffer{"unchanged_offer", 7});
        std::mt19937 poorEngine(456u);
        const autochess::core::PlayerState poorPlayerBefore = poorPlayer;
        const autochess::core::ShopState poorShopBefore = poorShop;
        const std::mt19937 poorEngineBefore = poorEngine;
        const auto poorResult = autochess::core::ShopService::refresh(
            poorPlayer,
            poorShop,
            gameConfig,
            units,
            faction,
            noModifiers,
            poorEngine);
        runner.check(
            !poorResult.success
                && poorResult.errorCode
                    == autochess::core::CommandErrorCode::InsufficientGold
                && !poorResult.message.empty()
                && playersAreEqual(poorPlayer, poorPlayerBefore)
                && shopsAreEqual(poorShop, poorShopBefore)
                && poorEngine == poorEngineBefore,
            "ShopService leaves all state unchanged when refresh gold is insufficient");

        const std::vector<autochess::core::UnitDefinition> emptyUnitPool;
        autochess::core::ShopState emptyPoolShop = poorShopBefore;
        std::mt19937 emptyPoolEngine(789u);
        const autochess::core::ShopState emptyPoolShopBefore = emptyPoolShop;
        const std::mt19937 emptyPoolEngineBefore = emptyPoolEngine;
        const auto emptyPoolResult = autochess::core::ShopService::rebuild(
            player,
            emptyPoolShop,
            gameConfig,
            emptyUnitPool,
            faction,
            noModifiers,
            emptyPoolEngine);
        runner.check(
            !emptyPoolResult.success
                && emptyPoolResult.errorCode
                    == autochess::core::CommandErrorCode::InvalidConfiguration
                && !emptyPoolResult.message.empty()
                && shopsAreEqual(emptyPoolShop, emptyPoolShopBefore)
                && emptyPoolEngine == emptyPoolEngineBefore,
            "ShopService rejects an empty unit pool without changing state");

        auto invalidPriceUnits = singleUnitPool;
        invalidPriceUnits.front().price = 0;
        autochess::core::PlayerState invalidPricePlayer = player;
        autochess::core::ShopState invalidPriceShop = poorShopBefore;
        std::mt19937 invalidPriceEngine(987u);
        const autochess::core::PlayerState invalidPricePlayerBefore =
            invalidPricePlayer;
        const autochess::core::ShopState invalidPriceShopBefore =
            invalidPriceShop;
        const std::mt19937 invalidPriceEngineBefore = invalidPriceEngine;
        const auto invalidPriceResult = autochess::core::ShopService::refresh(
            invalidPricePlayer,
            invalidPriceShop,
            gameConfig,
            invalidPriceUnits,
            faction,
            noModifiers,
            invalidPriceEngine);
        runner.check(
            !invalidPriceResult.success
                && invalidPriceResult.errorCode
                    == autochess::core::CommandErrorCode::InvalidConfiguration
                && !invalidPriceResult.message.empty()
                && playersAreEqual(
                    invalidPricePlayer,
                    invalidPricePlayerBefore)
                && shopsAreEqual(invalidPriceShop, invalidPriceShopBefore)
                && invalidPriceEngine == invalidPriceEngineBefore,
            "ShopService rejects invalid prices without changing state");

        autochess::core::PlayerState mismatchedPlayer = player;
        mismatchedPlayer.factionId = "other_team";
        autochess::core::ShopState mismatchedShop = poorShopBefore;
        std::mt19937 mismatchedEngine(654u);
        const autochess::core::PlayerState mismatchedPlayerBefore =
            mismatchedPlayer;
        const autochess::core::ShopState mismatchedShopBefore =
            mismatchedShop;
        const std::mt19937 mismatchedEngineBefore = mismatchedEngine;
        const auto mismatchResult = autochess::core::ShopService::refresh(
            mismatchedPlayer,
            mismatchedShop,
            gameConfig,
            units,
            faction,
            noModifiers,
            mismatchedEngine);
        runner.check(
            !mismatchResult.success
                && mismatchResult.errorCode
                    == autochess::core::CommandErrorCode::InconsistentState
                && !mismatchResult.message.empty()
                && playersAreEqual(mismatchedPlayer, mismatchedPlayerBefore)
                && shopsAreEqual(mismatchedShop, mismatchedShopBefore)
                && mismatchedEngine == mismatchedEngineBefore,
            "ShopService rejects a mismatched faction without changing state");

        auto invalidRefreshConfig = gameConfig;
        invalidRefreshConfig.shopRefreshCost = -1;
        autochess::core::PlayerState invalidRefreshPlayer = player;
        autochess::core::ShopState invalidRefreshShop = poorShopBefore;
        std::mt19937 invalidRefreshEngine(321u);
        const autochess::core::PlayerState invalidRefreshPlayerBefore =
            invalidRefreshPlayer;
        const autochess::core::ShopState invalidRefreshShopBefore =
            invalidRefreshShop;
        const std::mt19937 invalidRefreshEngineBefore = invalidRefreshEngine;
        const auto invalidRefreshResult =
            autochess::core::ShopService::refresh(
                invalidRefreshPlayer,
                invalidRefreshShop,
                invalidRefreshConfig,
                units,
                faction,
                noModifiers,
                invalidRefreshEngine);
        runner.check(
            !invalidRefreshResult.success
                && invalidRefreshResult.errorCode
                    == autochess::core::CommandErrorCode::InvalidConfiguration
                && !invalidRefreshResult.message.empty()
                && playersAreEqual(
                    invalidRefreshPlayer,
                    invalidRefreshPlayerBefore)
                && shopsAreEqual(
                    invalidRefreshShop,
                    invalidRefreshShopBefore)
                && invalidRefreshEngine == invalidRefreshEngineBefore,
            "ShopService rejects a negative refresh cost without changing state");

        return runner.failureCount();
    }

    int runShopPurchaseTests()
    {
        TestRunner runner;

        autochess::core::GameConfig gameConfig;
        gameConfig.startingGold = 10;
        gameConfig.rosterCapacity = 2;

        autochess::core::FactionDefinition faction;
        faction.id = "purchase_team";
        faction.name = "购买测试分队";
        faction.initialGuard = 100;
        faction.maxDeployed = 1;
        faction.priceMultiplier = 1.0;

        const std::vector<autochess::core::UnitDefinition> units = {
            [] {
                autochess::core::UnitDefinition unit;
                unit.id = "cheap_unit";
                unit.price = 2;
                return unit;
            }(),
            [] {
                autochess::core::UnitDefinition unit;
                unit.id = "normal_unit";
                unit.price = 3;
                return unit;
            }(),
            [] {
                autochess::core::UnitDefinition unit;
                unit.id = "expensive_unit";
                unit.price = 5;
                return unit;
            }()};

        autochess::core::PlayerState player =
            autochess::core::PlayerStateService::createInitial(
                autochess::core::MapSide::A,
                gameConfig,
                faction);
        autochess::core::ShopState shop;
        shop.offers.resize(3);
        shop.offers[0] =
            autochess::core::ShopOffer{"cheap_unit", 2};
        shop.offers[1] =
            autochess::core::ShopOffer{"normal_unit", 3};
        autochess::core::OwnedUnitId nextOwnedUnitId = 1;

        const auto firstPurchase =
            autochess::core::ShopService::purchase(
                player,
                shop,
                0,
                units,
                nextOwnedUnitId);
        std::string validationError;
        const auto* firstUnit =
            autochess::core::PlayerStateService::findActive(player, 1);
        runner.check(
            firstPurchase.success
                && firstPurchase.errorCode
                    == autochess::core::CommandErrorCode::None
                && !firstPurchase.message.empty()
                && player.gold == 8
                && player.activeUnits.size() == 1
                && firstUnit != nullptr
                && firstUnit->identity
                    == autochess::core::UnitIdentity{"cheap_unit", 1}
                && firstUnit->ownerSide == autochess::core::MapSide::A
                && player.reserveSlots[0].has_value()
                && player.reserveSlots[0].value() == 1
                && !shop.offers[0].has_value()
                && nextOwnedUnitId == 2
                && autochess::core::PlayerStateService::validate(
                    player,
                    validationError),
            "ShopService purchase creates a level-one unit in the first reserve slot");

        const auto secondPurchase =
            autochess::core::ShopService::purchase(
                player,
                shop,
                1,
                units,
                nextOwnedUnitId);
        validationError.clear();
        const auto* secondUnit =
            autochess::core::PlayerStateService::findActive(player, 2);
        runner.check(
            secondPurchase.success
                && player.gold == 5
                && player.activeUnits.size() == 2
                && secondUnit != nullptr
                && secondUnit->identity
                    == autochess::core::UnitIdentity{"normal_unit", 1}
                && player.reserveSlots[1].has_value()
                && player.reserveSlots[1].value() == 2
                && !shop.offers[1].has_value()
                && nextOwnedUnitId == 3
                && autochess::core::PlayerStateService::validate(
                    player,
                    validationError),
            "ShopService purchase uses the next empty reserve slot and ID");

        auto makeSingleOfferShop = [] {
            autochess::core::ShopState result;
            result.offers.resize(1);
            result.offers[0] =
                autochess::core::ShopOffer{"cheap_unit", 2};
            return result;
        };

        auto poorPlayer =
            autochess::core::PlayerStateService::createInitial(
                autochess::core::MapSide::A,
                gameConfig,
                faction);
        poorPlayer.gold = 1;
        auto poorShop = makeSingleOfferShop();
        const auto poorPlayerBefore = poorPlayer;
        const auto poorShopBefore = poorShop;
        autochess::core::OwnedUnitId poorNextId = 20;
        const auto poorNextIdBefore = poorNextId;
        const auto poorResult = autochess::core::ShopService::purchase(
            poorPlayer,
            poorShop,
            0,
            units,
            poorNextId);
        runner.check(
            !poorResult.success
                && poorResult.errorCode
                    == autochess::core::CommandErrorCode::InsufficientGold
                && !poorResult.message.empty()
                && playersAreEqual(poorPlayer, poorPlayerBefore)
                && shopsAreEqual(poorShop, poorShopBefore)
                && poorNextId == poorNextIdBefore,
            "ShopService rejects insufficient purchase gold atomically");

        auto fullPlayer = player;
        auto fullShop = makeSingleOfferShop();
        const auto fullPlayerBefore = fullPlayer;
        const auto fullShopBefore = fullShop;
        autochess::core::OwnedUnitId fullNextId = 20;
        const auto fullNextIdBefore = fullNextId;
        const auto fullResult = autochess::core::ShopService::purchase(
            fullPlayer,
            fullShop,
            0,
            units,
            fullNextId);
        runner.check(
            !fullResult.success
                && fullResult.errorCode
                    == autochess::core::CommandErrorCode::RosterFull
                && !fullResult.message.empty()
                && playersAreEqual(fullPlayer, fullPlayerBefore)
                && shopsAreEqual(fullShop, fullShopBefore)
                && fullNextId == fullNextIdBefore,
            "ShopService rejects a full roster atomically");

        auto invalidSlotPlayer =
            autochess::core::PlayerStateService::createInitial(
                autochess::core::MapSide::A,
                gameConfig,
                faction);
        auto invalidSlotShop = makeSingleOfferShop();
        const auto invalidSlotPlayerBefore = invalidSlotPlayer;
        const auto invalidSlotShopBefore = invalidSlotShop;
        autochess::core::OwnedUnitId invalidSlotNextId = 30;
        const auto invalidSlotNextIdBefore = invalidSlotNextId;
        const auto invalidSlotResult = autochess::core::ShopService::purchase(
            invalidSlotPlayer,
            invalidSlotShop,
            invalidSlotShop.offers.size(),
            units,
            invalidSlotNextId);
        runner.check(
            !invalidSlotResult.success
                && invalidSlotResult.errorCode
                    == autochess::core::CommandErrorCode::InvalidShopSlot
                && playersAreEqual(
                    invalidSlotPlayer,
                    invalidSlotPlayerBefore)
                && shopsAreEqual(invalidSlotShop, invalidSlotShopBefore)
                && invalidSlotNextId == invalidSlotNextIdBefore,
            "ShopService rejects an out-of-range purchase slot");

        auto emptySlotPlayer =
            autochess::core::PlayerStateService::createInitial(
                autochess::core::MapSide::A,
                gameConfig,
                faction);
        auto emptySlotShop = makeSingleOfferShop();
        emptySlotShop.offers[0].reset();
        const auto emptySlotPlayerBefore = emptySlotPlayer;
        const auto emptySlotShopBefore = emptySlotShop;
        autochess::core::OwnedUnitId emptySlotNextId = 31;
        const auto emptySlotNextIdBefore = emptySlotNextId;
        const auto emptySlotResult = autochess::core::ShopService::purchase(
            emptySlotPlayer,
            emptySlotShop,
            0,
            units,
            emptySlotNextId);
        runner.check(
            !emptySlotResult.success
                && emptySlotResult.errorCode
                    == autochess::core::CommandErrorCode::EmptyShopSlot
                && playersAreEqual(emptySlotPlayer, emptySlotPlayerBefore)
                && shopsAreEqual(emptySlotShop, emptySlotShopBefore)
                && emptySlotNextId == emptySlotNextIdBefore,
            "ShopService rejects an empty purchase slot");

        auto missingUnitPlayer =
            autochess::core::PlayerStateService::createInitial(
                autochess::core::MapSide::A,
                gameConfig,
                faction);
        auto missingUnitShop = makeSingleOfferShop();
        missingUnitShop.offers[0]->unitId = "missing_unit";
        const auto missingUnitPlayerBefore = missingUnitPlayer;
        const auto missingUnitShopBefore = missingUnitShop;
        autochess::core::OwnedUnitId missingUnitNextId = 32;
        const auto missingUnitNextIdBefore = missingUnitNextId;
        const auto missingUnitResult = autochess::core::ShopService::purchase(
            missingUnitPlayer,
            missingUnitShop,
            0,
            units,
            missingUnitNextId);
        runner.check(
            !missingUnitResult.success
                && missingUnitResult.errorCode
                    == autochess::core::CommandErrorCode::UnitNotFound
                && playersAreEqual(
                    missingUnitPlayer,
                    missingUnitPlayerBefore)
                && shopsAreEqual(missingUnitShop, missingUnitShopBefore)
                && missingUnitNextId == missingUnitNextIdBefore,
            "ShopService rejects an offer with a missing unit definition");

        auto invalidPricePlayer =
            autochess::core::PlayerStateService::createInitial(
                autochess::core::MapSide::A,
                gameConfig,
                faction);
        auto invalidPriceShop = makeSingleOfferShop();
        invalidPriceShop.offers[0]->displayedPrice = 0;
        const auto invalidPricePlayerBefore = invalidPricePlayer;
        const auto invalidPriceShopBefore = invalidPriceShop;
        autochess::core::OwnedUnitId invalidPriceNextId = 33;
        const auto invalidPriceNextIdBefore = invalidPriceNextId;
        const auto invalidPriceResult = autochess::core::ShopService::purchase(
            invalidPricePlayer,
            invalidPriceShop,
            0,
            units,
            invalidPriceNextId);
        runner.check(
            !invalidPriceResult.success
                && invalidPriceResult.errorCode
                    == autochess::core::CommandErrorCode::InvalidConfiguration
                && playersAreEqual(
                    invalidPricePlayer,
                    invalidPricePlayerBefore)
                && shopsAreEqual(invalidPriceShop, invalidPriceShopBefore)
                && invalidPriceNextId == invalidPriceNextIdBefore,
            "ShopService rejects a non-positive displayed price");

        auto zeroIdPlayer =
            autochess::core::PlayerStateService::createInitial(
                autochess::core::MapSide::A,
                gameConfig,
                faction);
        auto zeroIdShop = makeSingleOfferShop();
        const auto zeroIdPlayerBefore = zeroIdPlayer;
        const auto zeroIdShopBefore = zeroIdShop;
        autochess::core::OwnedUnitId zeroNextId =
            autochess::core::InvalidOwnedUnitId;
        const auto zeroIdResult = autochess::core::ShopService::purchase(
            zeroIdPlayer,
            zeroIdShop,
            0,
            units,
            zeroNextId);
        runner.check(
            !zeroIdResult.success
                && zeroIdResult.errorCode
                    == autochess::core::CommandErrorCode::InconsistentState
                && playersAreEqual(zeroIdPlayer, zeroIdPlayerBefore)
                && shopsAreEqual(zeroIdShop, zeroIdShopBefore)
                && zeroNextId == autochess::core::InvalidOwnedUnitId,
            "ShopService rejects an invalid next owned-unit ID");

        auto duplicateIdPlayer = player;
        duplicateIdPlayer.activeUnits.pop_back();
        duplicateIdPlayer.reserveSlots[1].reset();
        auto duplicateIdShop = makeSingleOfferShop();
        const auto duplicateIdPlayerBefore = duplicateIdPlayer;
        const auto duplicateIdShopBefore = duplicateIdShop;
        autochess::core::OwnedUnitId duplicateNextId = 1;
        const auto duplicateIdResult = autochess::core::ShopService::purchase(
            duplicateIdPlayer,
            duplicateIdShop,
            0,
            units,
            duplicateNextId);
        runner.check(
            !duplicateIdResult.success
                && duplicateIdResult.errorCode
                    == autochess::core::CommandErrorCode::InconsistentState
                && playersAreEqual(
                    duplicateIdPlayer,
                    duplicateIdPlayerBefore)
                && shopsAreEqual(duplicateIdShop, duplicateIdShopBefore)
                && duplicateNextId == 1,
            "ShopService rejects an already-used owned-unit ID");

        auto invalidStatePlayer = player;
        invalidStatePlayer.deployments.emplace(
            autochess::core::GridPosition{1, 1},
            invalidStatePlayer.activeUnits.front().id);
        auto invalidStateShop = makeSingleOfferShop();
        const auto invalidStatePlayerBefore = invalidStatePlayer;
        const auto invalidStateShopBefore = invalidStateShop;
        autochess::core::OwnedUnitId invalidStateNextId = 40;
        const auto invalidStateResult = autochess::core::ShopService::purchase(
            invalidStatePlayer,
            invalidStateShop,
            0,
            units,
            invalidStateNextId);
        runner.check(
            !invalidStateResult.success
                && invalidStateResult.errorCode
                    == autochess::core::CommandErrorCode::InconsistentState
                && playersAreEqual(
                    invalidStatePlayer,
                    invalidStatePlayerBefore)
                && shopsAreEqual(invalidStateShop, invalidStateShopBefore)
                && invalidStateNextId == 40,
            "ShopService rejects an already-invalid player state");

        return runner.failureCount();
    }

    bool runPlayerStateServiceTests()
    {
        TestRunner runner;
        autochess::core::ConfigBundle bundle;
        autochess::core::ConfigError loadError;
        const bool loaded = autochess::core::ConfigBundleLoader::load(
            AUTOCHESS_DATA_DIR,
            bundle,
            loadError);
        runner.check(
            loaded,
            "PlayerStateService loads the formal configuration bundle");
        if (!loaded)
        {
            return runner.failureCount();
        }

        const autochess::core::FactionDefinition* trainingFaction = nullptr;
        for (const autochess::core::FactionDefinition& faction : bundle.factions)
        {
            if (faction.id == "training_team")
            {
                trainingFaction = &faction;
                break;
            }
        }

        runner.check(
            trainingFaction != nullptr,
            "PlayerStateService finds the training faction");
        if (trainingFaction == nullptr)
        {
            return runner.failureCount();
        }

        const autochess::core::PlayerState initial =
            autochess::core::PlayerStateService::createInitial(
                autochess::core::MapSide::A,
                bundle.gameConfig,
                *trainingFaction);

        runner.check(
            initial.side == autochess::core::MapSide::A
                && initial.factionId == "training_team"
                && initial.gold == 10
                && initial.guardValue == 100
                && initial.reserveSlots.size() == 8
                && initial.activeUnits.empty()
                && initial.deadUnits.empty()
                && initial.deployments.empty(),
            "PlayerStateService creates an empty configured player state");

        std::string validationError;
        runner.check(
            autochess::core::PlayerStateService::validate(
                initial,
                validationError)
                && validationError.empty(),
            "PlayerStateService accepts the initial player state");

        const autochess::core::UnitIdentity guardIdentity{
            "training_guard", 1};
        const autochess::core::OwnedUnit reserveUnit{
            1,
            guardIdentity,
            autochess::core::MapSide::A};
        const autochess::core::OwnedUnit firstDeployedUnit{
            2,
            guardIdentity,
            autochess::core::MapSide::A};
        const autochess::core::OwnedUnit secondDeployedUnit{
            3,
            guardIdentity,
            autochess::core::MapSide::A};
        const autochess::core::OwnedUnit deadUnit{
            4,
            {"training_guard", 2},
            autochess::core::MapSide::A};

        autochess::core::PlayerState player = initial;
        player.activeUnits = {
            reserveUnit,
            firstDeployedUnit,
            secondDeployedUnit};
        player.deadUnits = {deadUnit};
        player.reserveSlots[0] = reserveUnit.id;
        player.deployments.emplace(
            autochess::core::GridPosition{1, 2},
            firstDeployedUnit.id);
        player.deployments.emplace(
            autochess::core::GridPosition{1, 4},
            secondDeployedUnit.id);

        const autochess::core::OwnedUnit* activeReserveResult =
            autochess::core::PlayerStateService::findActive(player, 1);
        const autochess::core::OwnedUnit* activeDeploymentResult =
            autochess::core::PlayerStateService::findActive(player, 2);
        const autochess::core::OwnedUnit* deadResult =
            autochess::core::PlayerStateService::findDead(player, 4);
        runner.check(
            activeReserveResult != nullptr
                && activeReserveResult->id == reserveUnit.id
                && activeDeploymentResult != nullptr
                && activeDeploymentResult->id == firstDeployedUnit.id
                && deadResult != nullptr
                && deadResult->id == deadUnit.id
                && autochess::core::PlayerStateService::findActive(player, 4)
                    == nullptr
                && autochess::core::PlayerStateService::findDead(player, 1)
                    == nullptr
                && autochess::core::PlayerStateService::findActive(player, 99)
                    == nullptr,
            "PlayerStateService finds active, dead, and missing units");

        const std::optional<std::size_t> reserveSlot =
            autochess::core::PlayerStateService::findReserveSlot(player, 1);
        const std::optional<std::size_t> deployedReserveSlot =
            autochess::core::PlayerStateService::findReserveSlot(player, 2);
        const std::optional<std::size_t> deadReserveSlot =
            autochess::core::PlayerStateService::findReserveSlot(player, 4);
        runner.check(
            reserveSlot.has_value()
                && reserveSlot.value() == 0
                && !deployedReserveSlot.has_value()
                && !deadReserveSlot.has_value(),
            "PlayerStateService finds reserve slots only for reserve units");

        const std::optional<autochess::core::GridPosition> firstPosition =
            autochess::core::PlayerStateService::findDeploymentPosition(
                player,
                2);
        const std::optional<autochess::core::GridPosition> reservePosition =
            autochess::core::PlayerStateService::findDeploymentPosition(
                player,
                1);
        runner.check(
            firstPosition.has_value()
                && firstPosition.value()
                    == autochess::core::GridPosition{1, 2}
                && !reservePosition.has_value(),
            "PlayerStateService finds deployment positions only for deployed units");

        const std::optional<std::size_t> firstEmptySlot =
            autochess::core::PlayerStateService::findFirstEmptyReserveSlot(
                player);
        runner.check(
            firstEmptySlot.has_value() && firstEmptySlot.value() == 1,
            "PlayerStateService finds the first empty reserve slot");

        autochess::core::PlayerState fullReserve = player;
        for (std::size_t slot = 0; slot < fullReserve.reserveSlots.size(); ++slot)
        {
            fullReserve.reserveSlots[slot] =
                static_cast<autochess::core::OwnedUnitId>(100 + slot);
        }
        runner.check(
            !autochess::core::PlayerStateService::findFirstEmptyReserveSlot(
                fullReserve)
                 .has_value(),
            "PlayerStateService reports no empty slot when reserve is full");

        runner.check(
            autochess::core::PlayerStateService::activeCount(player) == 3
                && autochess::core::PlayerStateService::deployedCount(player)
                    == 2,
            "PlayerStateService counts active and deployed units");

        runner.check(
            autochess::core::PlayerStateService::validate(
                player,
                validationError)
                && validationError.empty(),
            "PlayerStateService accepts a consistent mixed player state");

        autochess::core::PlayerState duplicateActive = player;
        duplicateActive.activeUnits.push_back(reserveUnit);
        runner.check(
            !autochess::core::PlayerStateService::validate(
                duplicateActive,
                validationError)
                && !validationError.empty(),
            "PlayerStateService rejects duplicate active IDs");

        autochess::core::PlayerState duplicateLocation = player;
        duplicateLocation.reserveSlots[1] = firstDeployedUnit.id;
        runner.check(
            !autochess::core::PlayerStateService::validate(
                duplicateLocation,
                validationError),
            "PlayerStateService rejects a unit in reserve and deployment");

        autochess::core::PlayerState missingActiveReference = player;
        missingActiveReference.deployments.emplace(
            autochess::core::GridPosition{2, 2},
            99);
        runner.check(
            !autochess::core::PlayerStateService::validate(
                missingActiveReference,
                validationError),
            "PlayerStateService rejects an unknown deployment ID");

        autochess::core::PlayerState deadInReserve = player;
        deadInReserve.reserveSlots[1] = deadUnit.id;
        runner.check(
            !autochess::core::PlayerStateService::validate(
                deadInReserve,
                validationError),
            "PlayerStateService rejects a dead unit in reserve");

        autochess::core::PlayerState wrongOwner = player;
        wrongOwner.activeUnits[0].ownerSide = autochess::core::MapSide::B;
        runner.check(
            !autochess::core::PlayerStateService::validate(
                wrongOwner,
                validationError),
            "PlayerStateService rejects an active unit with wrong owner");

        autochess::core::PlayerState overCapacity = player;
        overCapacity.reserveSlots.resize(2);
        runner.check(
            !autochess::core::PlayerStateService::validate(
                overCapacity,
                validationError),
            "PlayerStateService rejects active units over capacity");

        autochess::core::PlayerState invalidId = player;
        invalidId.activeUnits[0].id =
            autochess::core::InvalidOwnedUnitId;
        runner.check(
            !autochess::core::PlayerStateService::validate(
                invalidId,
                validationError),
            "PlayerStateService rejects an invalid persistent ID");

        return runner.failureCount();
    }

    int runDeploymentServiceTests()
    {
        TestRunner runner;

        autochess::core::ConfigBundle bundle;
        autochess::core::ConfigError loadError;
        const bool loaded = autochess::core::ConfigBundleLoader::load(
            AUTOCHESS_DATA_DIR,
            bundle,
            loadError);
        runner.check(
            loaded,
            "DeploymentService loads the formal configuration bundle");
        if (!loaded)
        {
            return runner.failureCount();
        }

        const autochess::core::MapDefinition* map = nullptr;
        const autochess::core::FactionDefinition* trainingFaction = nullptr;
        for (const autochess::core::MapDefinition& candidate : bundle.maps)
        {
            if (candidate.id == "map_01")
            {
                map = &candidate;
                break;
            }
        }
        for (const autochess::core::FactionDefinition& candidate : bundle.factions)
        {
            if (candidate.id == "training_team")
            {
                trainingFaction = &candidate;
                break;
            }
        }

        runner.check(
            map != nullptr && trainingFaction != nullptr,
            "DeploymentService finds map_01 and the training faction");
        if (map == nullptr || trainingFaction == nullptr)
        {
            return runner.failureCount();
        }

        const auto makePlayer = [&]()
        {
            autochess::core::PlayerState player =
                autochess::core::PlayerStateService::createInitial(
                    autochess::core::MapSide::A,
                    bundle.gameConfig,
                    *trainingFaction);
            const autochess::core::UnitIdentity identity{
                "training_guard", 1};
            player.activeUnits = {
                autochess::core::OwnedUnit{
                    1, identity, autochess::core::MapSide::A},
                autochess::core::OwnedUnit{
                    2, identity, autochess::core::MapSide::A}};
            player.reserveSlots[0] = 1;
            player.reserveSlots[1] = 2;
            return player;
        };

        auto player = makePlayer();
        std::string validationError;
        runner.check(
            autochess::core::PlayerStateService::validate(
                player,
                validationError),
            "DeploymentService test player starts in a valid state");

        const auto deployResult =
            autochess::core::DeploymentService::moveToDeployment(
                player,
                *map,
                *trainingFaction,
                1,
                autochess::core::GridPosition{1, 2});
        runner.check(
            deployResult.success
                && deployResult.errorCode
                    == autochess::core::CommandErrorCode::None
                && !deployResult.message.empty()
                && !player.reserveSlots[0].has_value()
                && player.deployments.at({1, 2}) == 1
                && player.deployments.size() == 1
                && autochess::core::PlayerStateService::validate(
                    player,
                    validationError),
            "DeploymentService deploys a reserve unit to a legal start");

        auto movedPlayer = player;
        const auto moveResult =
            autochess::core::DeploymentService::moveToDeployment(
                movedPlayer,
                *map,
                *trainingFaction,
                1,
                autochess::core::GridPosition{1, 4});
        runner.check(
            moveResult.success
                && movedPlayer.deployments.find({1, 2})
                    == movedPlayer.deployments.end()
                && movedPlayer.deployments.at({1, 4}) == 1
                && movedPlayer.deployments.size() == 1,
            "DeploymentService moves a deployed unit to an empty start");

        auto reservePlayer = movedPlayer;
        const auto reserveResult =
            autochess::core::DeploymentService::moveToReserve(
                reservePlayer,
                1,
                2);
        runner.check(
            reserveResult.success
                && reservePlayer.deployments.find({1, 4})
                    == reservePlayer.deployments.end()
                && reservePlayer.reserveSlots[2].has_value()
                && reservePlayer.reserveSlots[2].value() == 1
                && reservePlayer.deployments.empty()
                && autochess::core::PlayerStateService::validate(
                    reservePlayer,
                    validationError),
            "DeploymentService withdraws a deployed unit to the reserve");

        auto reserveMovePlayer = makePlayer();
        const auto reserveMoveResult =
            autochess::core::DeploymentService::moveToReserve(
                reserveMovePlayer,
                2,
                3);
        runner.check(
            reserveMoveResult.success
                && !reserveMovePlayer.reserveSlots[1].has_value()
                && reserveMovePlayer.reserveSlots[3].has_value()
                && reserveMovePlayer.reserveSlots[3].value() == 2
                && reserveMovePlayer.activeUnits.size() == 2
                && autochess::core::PlayerStateService::validate(
                    reserveMovePlayer,
                    validationError),
            "DeploymentService moves a reserve unit between reserve slots");

        const auto expectUnchanged = [&runner](
            const autochess::core::CommandResult& result,
            const autochess::core::CommandErrorCode expectedCode,
            const autochess::core::PlayerState& actual,
            const autochess::core::PlayerState& before,
            const std::string& testName)
        {
            runner.check(
                !result.success
                    && result.errorCode == expectedCode
                    && !result.message.empty()
                    && playersAreEqual(actual, before),
                testName);
        };

        const auto sameDeploymentBefore = player;
        const auto sameDeploymentResult =
            autochess::core::DeploymentService::moveToDeployment(
                player,
                *map,
                *trainingFaction,
                1,
                autochess::core::GridPosition{1, 2});
        expectUnchanged(
            sameDeploymentResult,
            autochess::core::CommandErrorCode::InvalidTarget,
            player,
            sameDeploymentBefore,
            "DeploymentService rejects the unit's current deployment start");

        auto sameReservePlayer = makePlayer();
        const auto sameReserveBefore = sameReservePlayer;
        const auto sameReserveResult =
            autochess::core::DeploymentService::moveToReserve(
                sameReservePlayer,
                1,
                0);
        expectUnchanged(
            sameReserveResult,
            autochess::core::CommandErrorCode::InvalidTarget,
            sameReservePlayer,
            sameReserveBefore,
            "DeploymentService rejects the unit's current reserve slot");

        auto mismatchedFaction = *trainingFaction;
        mismatchedFaction.id = "other_team";
        auto mismatchedFactionPlayer = makePlayer();
        const auto mismatchedFactionBefore = mismatchedFactionPlayer;
        const auto mismatchedFactionResult =
            autochess::core::DeploymentService::moveToDeployment(
                mismatchedFactionPlayer,
                *map,
                mismatchedFaction,
                1,
                autochess::core::GridPosition{1, 2});
        expectUnchanged(
            mismatchedFactionResult,
            autochess::core::CommandErrorCode::InconsistentState,
            mismatchedFactionPlayer,
            mismatchedFactionBefore,
            "DeploymentService rejects a mismatched faction");

        auto invalidFaction = *trainingFaction;
        invalidFaction.maxDeployed = -1;
        auto invalidFactionPlayer = makePlayer();
        const auto invalidFactionBefore = invalidFactionPlayer;
        const auto invalidFactionResult =
            autochess::core::DeploymentService::moveToDeployment(
                invalidFactionPlayer,
                *map,
                invalidFaction,
                1,
                autochess::core::GridPosition{1, 2});
        expectUnchanged(
            invalidFactionResult,
            autochess::core::CommandErrorCode::InvalidConfiguration,
            invalidFactionPlayer,
            invalidFactionBefore,
            "DeploymentService rejects a negative deployment limit");

        const std::vector<autochess::core::GridPosition> invalidTargets = {
            {9, 2},
            {2, 2},
            {1, 3},
            {4, 2}};
        const std::vector<std::string> invalidTargetNames = {
            "DeploymentService rejects the opposing deployment start",
            "DeploymentService rejects an ordinary route cell",
            "DeploymentService rejects a guard cell",
            "DeploymentService rejects an obstacle cell"};
        for (std::size_t index = 0; index < invalidTargets.size(); ++index)
        {
            auto invalidTargetPlayer = makePlayer();
            const auto before = invalidTargetPlayer;
            const auto result =
                autochess::core::DeploymentService::moveToDeployment(
                    invalidTargetPlayer,
                    *map,
                    *trainingFaction,
                    1,
                    invalidTargets[index]);
            expectUnchanged(
                result,
                autochess::core::CommandErrorCode::InvalidTarget,
                invalidTargetPlayer,
                before,
                invalidTargetNames[index]);
        }

        auto occupiedPlayer = makePlayer();
        const auto firstOccupiedDeployment =
            autochess::core::DeploymentService::moveToDeployment(
                occupiedPlayer,
                *map,
                *trainingFaction,
                1,
                autochess::core::GridPosition{1, 2});
        runner.check(
            firstOccupiedDeployment.success,
            "DeploymentService prepares an occupied deployment test");
        const auto occupiedBefore = occupiedPlayer;
        const auto occupiedResult =
            autochess::core::DeploymentService::moveToDeployment(
                occupiedPlayer,
                *map,
                *trainingFaction,
                2,
                autochess::core::GridPosition{1, 2});
        expectUnchanged(
            occupiedResult,
            autochess::core::CommandErrorCode::TargetOccupied,
            occupiedPlayer,
            occupiedBefore,
            "DeploymentService rejects an occupied deployment start");

        auto limitedFaction = *trainingFaction;
        limitedFaction.maxDeployed = 1;
        auto limitedPlayer = makePlayer();
        const auto firstLimitedDeployment =
            autochess::core::DeploymentService::moveToDeployment(
                limitedPlayer,
                *map,
                limitedFaction,
                1,
                autochess::core::GridPosition{1, 2});
        runner.check(
            firstLimitedDeployment.success,
            "DeploymentService prepares a deployment-limit test");
        const auto limitedBefore = limitedPlayer;
        const auto limitedResult =
            autochess::core::DeploymentService::moveToDeployment(
                limitedPlayer,
                *map,
                limitedFaction,
                2,
                autochess::core::GridPosition{1, 4});
        expectUnchanged(
            limitedResult,
            autochess::core::CommandErrorCode::DeploymentLimitReached,
            limitedPlayer,
            limitedBefore,
            "DeploymentService enforces the faction deployment limit");

        auto missingUnitPlayer = makePlayer();
        const auto missingUnitBefore = missingUnitPlayer;
        const auto missingUnitResult =
            autochess::core::DeploymentService::moveToDeployment(
                missingUnitPlayer,
                *map,
                *trainingFaction,
                99,
                autochess::core::GridPosition{1, 2});
        expectUnchanged(
            missingUnitResult,
            autochess::core::CommandErrorCode::UnitNotFound,
            missingUnitPlayer,
            missingUnitBefore,
            "DeploymentService rejects a missing unit ID");

        auto invalidReservePlayer = makePlayer();
        const auto invalidReserveBefore = invalidReservePlayer;
        const auto invalidReserveResult =
            autochess::core::DeploymentService::moveToReserve(
                invalidReservePlayer,
                1,
                invalidReservePlayer.reserveSlots.size());
        expectUnchanged(
            invalidReserveResult,
            autochess::core::CommandErrorCode::InvalidTarget,
            invalidReservePlayer,
            invalidReserveBefore,
            "DeploymentService rejects an out-of-range reserve slot");

        auto occupiedReservePlayer = makePlayer();
        const auto occupiedReserveBefore = occupiedReservePlayer;
        const auto occupiedReserveResult =
            autochess::core::DeploymentService::moveToReserve(
                occupiedReservePlayer,
                2,
                0);
        expectUnchanged(
            occupiedReserveResult,
            autochess::core::CommandErrorCode::TargetOccupied,
            occupiedReservePlayer,
            occupiedReserveBefore,
            "DeploymentService rejects an occupied reserve slot");

        auto invalidStatePlayer = makePlayer();
        invalidStatePlayer.reserveSlots[1] = 1;
        const auto invalidStateBefore = invalidStatePlayer;
        const auto invalidStateResult =
            autochess::core::DeploymentService::moveToDeployment(
                invalidStatePlayer,
                *map,
                *trainingFaction,
                1,
                autochess::core::GridPosition{1, 2});
        expectUnchanged(
            invalidStateResult,
            autochess::core::CommandErrorCode::InconsistentState,
            invalidStatePlayer,
            invalidStateBefore,
            "DeploymentService rejects an invalid player state atomically");

        auto deadPlayer = makePlayer();
        deadPlayer.deadUnits.push_back(
            autochess::core::OwnedUnit{
                3,
                {"training_guard", 1},
                autochess::core::MapSide::A});
        const auto deadBefore = deadPlayer;
        const auto deadResult =
            autochess::core::DeploymentService::moveToReserve(
                deadPlayer,
                3,
                2);
        expectUnchanged(
            deadResult,
            autochess::core::CommandErrorCode::UnitNotFound,
            deadPlayer,
            deadBefore,
            "DeploymentService does not move a dead unit");

        return runner.failureCount();
    }

    // 此函数验证同类型同等级持久单位能够原子地合成为高一级单位。
    int runMergeServiceTests()
    {
        // 此代码段加载正式配置，以便合成测试复用真实价格、比例、分队和地图数据。
        TestRunner runner;
        autochess::core::ConfigBundle bundle;
        autochess::core::ConfigError loadError;
        const bool loaded = autochess::core::ConfigBundleLoader::load(
            AUTOCHESS_DATA_DIR,
            bundle,
            loadError);
        runner.check(
            loaded,
            "MergeService loads the formal configuration bundle");
        if (!loaded)
        {
            return runner.failureCount();
        }

        // 此代码段查找训练分队和第一张地图，供价格计算与部署目标测试使用。
        const autochess::core::FactionDefinition* trainingFaction = nullptr;
        const autochess::core::MapDefinition* map = nullptr;
        for (const autochess::core::FactionDefinition& candidate :
             bundle.factions)
        {
            if (candidate.id == "training_team")
            {
                trainingFaction = &candidate;
                break;
            }
        }
        for (const autochess::core::MapDefinition& candidate : bundle.maps)
        {
            if (candidate.id == "map_01")
            {
                map = &candidate;
                break;
            }
        }
        runner.check(
            trainingFaction != nullptr && map != nullptr,
            "MergeService finds the training faction and map_01");
        if (trainingFaction == nullptr || map == nullptr)
        {
            return runner.failureCount();
        }

        // 此代码段创建两个同类型同等级备用单位，作为各合成案例的独立初始状态。
        const auto makePlayer = [&](const int level)
        {
            autochess::core::PlayerState player =
                autochess::core::PlayerStateService::createInitial(
                    autochess::core::MapSide::A,
                    bundle.gameConfig,
                    *trainingFaction);
            const autochess::core::UnitIdentity identity{
                "training_guard", level};
            player.activeUnits = {
                autochess::core::OwnedUnit{
                    1, identity, autochess::core::MapSide::A},
                autochess::core::OwnedUnit{
                    2, identity, autochess::core::MapSide::A}};
            player.reserveSlots[0] = 1;
            player.reserveSlots[1] = 2;
            return player;
        };

        // 此代码段验证两个一级备用单位生成新 ID 的二级单位并保留目标槽位。
        auto levelOnePlayer = makePlayer(1);
        const int levelOneGoldBefore = levelOnePlayer.gold;
        autochess::core::OwnedUnitId levelOneNextId = 3;
        std::string validationError;
        const auto levelOneResult = autochess::core::MergeService::merge(
            levelOnePlayer,
            1,
            2,
            bundle.gameConfig,
            bundle.units,
            *trainingFaction,
            bundle.factionModifiers,
            levelOneNextId);
        const autochess::core::OwnedUnit* levelTwoUnit =
            autochess::core::PlayerStateService::findActive(
                levelOnePlayer, 3);
        runner.check(
            levelOneResult.success
                && levelOneResult.errorCode
                    == autochess::core::CommandErrorCode::None
                && !levelOneResult.message.empty()
                && autochess::core::PlayerStateService::findActive(
                    levelOnePlayer, 1) == nullptr
                && autochess::core::PlayerStateService::findActive(
                    levelOnePlayer, 2) == nullptr
                && levelTwoUnit != nullptr
                && levelTwoUnit->identity.unitId == "training_guard"
                && levelTwoUnit->identity.level == 2
                && levelOnePlayer.activeUnits.size() == 1
                && !levelOnePlayer.reserveSlots[0].has_value()
                && levelOnePlayer.reserveSlots[1].has_value()
                && levelOnePlayer.reserveSlots[1].value() == 3
                && levelOnePlayer.gold == levelOneGoldBefore + 1
                && levelOneNextId == 4
                && autochess::core::PlayerStateService::validate(
                    levelOnePlayer,
                    validationError),
            "MergeService combines two level-one reserve units");

        // 此代码段验证两个二级单位生成三级单位且返还金额仍按一级价格计算。
        auto levelTwoPlayer = makePlayer(2);
        const int levelTwoGoldBefore = levelTwoPlayer.gold;
        autochess::core::OwnedUnitId levelTwoNextId = 3;
        const auto levelTwoResult = autochess::core::MergeService::merge(
            levelTwoPlayer,
            1,
            2,
            bundle.gameConfig,
            bundle.units,
            *trainingFaction,
            bundle.factionModifiers,
            levelTwoNextId);
        const autochess::core::OwnedUnit* levelThreeUnit =
            autochess::core::PlayerStateService::findActive(
                levelTwoPlayer, 3);
        validationError.clear();
        runner.check(
            levelTwoResult.success
                && levelThreeUnit != nullptr
                && levelThreeUnit->identity.level == 3
                && levelTwoPlayer.gold == levelTwoGoldBefore + 1
                && levelTwoPlayer.reserveSlots[1].has_value()
                && levelTwoPlayer.reserveSlots[1].value() == 3
                && levelTwoNextId == 4
                && autochess::core::PlayerStateService::validate(
                    levelTwoPlayer,
                    validationError),
            "MergeService combines two level-two units into level three");

        // 此代码段验证目标单位位于部署区时，新单位继承目标部署格而不返回备用区。
        auto deployedTargetPlayer = makePlayer(1);
        const auto deploymentPreparationResult =
            autochess::core::DeploymentService::moveToDeployment(
                deployedTargetPlayer,
                *map,
                *trainingFaction,
                2,
                autochess::core::GridPosition{1, 2});
        autochess::core::OwnedUnitId deployedTargetNextId = 3;
        const auto deployedTargetMergeResult =
            autochess::core::MergeService::merge(
                deployedTargetPlayer,
                1,
                2,
                bundle.gameConfig,
                bundle.units,
                *trainingFaction,
                bundle.factionModifiers,
                deployedTargetNextId);
        validationError.clear();
        runner.check(
            deploymentPreparationResult.success
                && deployedTargetMergeResult.success
                && !deployedTargetPlayer.reserveSlots[0].has_value()
                && !deployedTargetPlayer.reserveSlots[1].has_value()
                && deployedTargetPlayer.deployments.size() == 1
                && deployedTargetPlayer.deployments.at({1, 2}) == 3
                && deployedTargetPlayer.activeUnits.size() == 1
                && deployedTargetNextId == 4
                && autochess::core::PlayerStateService::validate(
                    deployedTargetPlayer,
                    validationError),
            "MergeService preserves the deployed target position");

        // 此代码段验证源单位在部署区而目标在备用区时，新单位继承目标备用槽位。
        auto reserveTargetPlayer = makePlayer(1);
        const auto sourceDeploymentResult =
            autochess::core::DeploymentService::moveToDeployment(
                reserveTargetPlayer,
                *map,
                *trainingFaction,
                1,
                autochess::core::GridPosition{1, 2});
        autochess::core::OwnedUnitId reserveTargetNextId = 3;
        const auto reserveTargetMergeResult =
            autochess::core::MergeService::merge(
                reserveTargetPlayer,
                1,
                2,
                bundle.gameConfig,
                bundle.units,
                *trainingFaction,
                bundle.factionModifiers,
                reserveTargetNextId);
        validationError.clear();
        runner.check(
            sourceDeploymentResult.success
                && reserveTargetMergeResult.success
                && reserveTargetPlayer.deployments.empty()
                && !reserveTargetPlayer.reserveSlots[0].has_value()
                && reserveTargetPlayer.reserveSlots[1].has_value()
                && reserveTargetPlayer.reserveSlots[1].value() == 3
                && reserveTargetPlayer.activeUnits.size() == 1
                && reserveTargetNextId == 4
                && autochess::core::PlayerStateService::validate(
                    reserveTargetPlayer,
                    validationError),
            "MergeService preserves the reserve target position");

        // 此代码段验证两个已部署单位合成后只保留目标部署格。
        auto twoDeployedPlayer = makePlayer(1);
        const auto firstDeploymentResult =
            autochess::core::DeploymentService::moveToDeployment(
                twoDeployedPlayer,
                *map,
                *trainingFaction,
                1,
                autochess::core::GridPosition{1, 2});
        const auto secondDeploymentResult =
            autochess::core::DeploymentService::moveToDeployment(
                twoDeployedPlayer,
                *map,
                *trainingFaction,
                2,
                autochess::core::GridPosition{1, 4});
        autochess::core::OwnedUnitId twoDeployedNextId = 3;
        const auto twoDeployedMergeResult =
            autochess::core::MergeService::merge(
                twoDeployedPlayer,
                1,
                2,
                bundle.gameConfig,
                bundle.units,
                *trainingFaction,
                bundle.factionModifiers,
                twoDeployedNextId);
        validationError.clear();
        runner.check(
            firstDeploymentResult.success
                && secondDeploymentResult.success
                && twoDeployedMergeResult.success
                && twoDeployedPlayer.deployments.size() == 1
                && twoDeployedPlayer.deployments.find({1, 2})
                    == twoDeployedPlayer.deployments.end()
                && twoDeployedPlayer.deployments.at({1, 4}) == 3
                && twoDeployedPlayer.activeUnits.size() == 1
                && twoDeployedNextId == 4
                && autochess::core::PlayerStateService::validate(
                    twoDeployedPlayer,
                    validationError),
            "MergeService combines two deployed units at the target start");

        // 此代码段统一验证失败结果、玩家状态和下一持久 ID 均保持不变。
        const auto expectUnchanged = [&runner](
            const autochess::core::CommandResult& result,
            const autochess::core::CommandErrorCode expectedCode,
            const autochess::core::PlayerState& actualPlayer,
            const autochess::core::PlayerState& playerBefore,
            const autochess::core::OwnedUnitId actualNextId,
            const autochess::core::OwnedUnitId nextIdBefore,
            const std::string& testName)
        {
            runner.check(
                !result.success
                    && result.errorCode == expectedCode
                    && !result.message.empty()
                    && playersAreEqual(actualPlayer, playerBefore)
                    && actualNextId == nextIdBefore,
                testName);
        };

        // 此代码段验证单位不能与自身合成。
        auto sameIdPlayer = makePlayer(1);
        const auto sameIdBefore = sameIdPlayer;
        autochess::core::OwnedUnitId sameIdNext = 3;
        const auto sameIdResult = autochess::core::MergeService::merge(
            sameIdPlayer, 1, 1, bundle.gameConfig, bundle.units,
            *trainingFaction, bundle.factionModifiers, sameIdNext);
        expectUnchanged(
            sameIdResult,
            autochess::core::CommandErrorCode::NotMergeable,
            sameIdPlayer,
            sameIdBefore,
            sameIdNext,
            3,
            "MergeService rejects merging a unit with itself");

        // 此代码段验证缺少源单位时合成失败且状态不变。
        auto missingSourcePlayer = makePlayer(1);
        const auto missingSourceBefore = missingSourcePlayer;
        autochess::core::OwnedUnitId missingSourceNext = 3;
        const auto missingSourceResult = autochess::core::MergeService::merge(
            missingSourcePlayer, 99, 2, bundle.gameConfig, bundle.units,
            *trainingFaction, bundle.factionModifiers, missingSourceNext);
        expectUnchanged(
            missingSourceResult,
            autochess::core::CommandErrorCode::UnitNotFound,
            missingSourcePlayer,
            missingSourceBefore,
            missingSourceNext,
            3,
            "MergeService rejects a missing source unit");

        // 此代码段验证缺少目标单位时合成失败且状态不变。
        auto missingTargetPlayer = makePlayer(1);
        const auto missingTargetBefore = missingTargetPlayer;
        autochess::core::OwnedUnitId missingTargetNext = 3;
        const auto missingTargetResult = autochess::core::MergeService::merge(
            missingTargetPlayer, 1, 99, bundle.gameConfig, bundle.units,
            *trainingFaction, bundle.factionModifiers, missingTargetNext);
        expectUnchanged(
            missingTargetResult,
            autochess::core::CommandErrorCode::UnitNotFound,
            missingTargetPlayer,
            missingTargetBefore,
            missingTargetNext,
            3,
            "MergeService rejects a missing target unit");

        // 此代码段将源单位合法移入死亡列表，以验证死亡单位不能参与合成。
        auto deadSourcePlayer = makePlayer(1);
        const auto deadSourceUnit = deadSourcePlayer.activeUnits.front();
        deadSourcePlayer.activeUnits.erase(
            deadSourcePlayer.activeUnits.begin());
        deadSourcePlayer.reserveSlots[0].reset();
        deadSourcePlayer.deadUnits.push_back(deadSourceUnit);
        const auto deadSourceBefore = deadSourcePlayer;
        autochess::core::OwnedUnitId deadSourceNext = 3;
        const auto deadSourceResult = autochess::core::MergeService::merge(
            deadSourcePlayer, 1, 2, bundle.gameConfig, bundle.units,
            *trainingFaction, bundle.factionModifiers, deadSourceNext);
        expectUnchanged(
            deadSourceResult,
            autochess::core::CommandErrorCode::UnitNotFound,
            deadSourcePlayer,
            deadSourceBefore,
            deadSourceNext,
            3,
            "MergeService rejects a dead source unit");

        // 此代码段验证不同单位类型不能合成。
        auto differentTypePlayer = makePlayer(1);
        differentTypePlayer.activeUnits[0].identity.unitId = "other_unit";
        const auto differentTypeBefore = differentTypePlayer;
        autochess::core::OwnedUnitId differentTypeNext = 3;
        const auto differentTypeResult = autochess::core::MergeService::merge(
            differentTypePlayer, 1, 2, bundle.gameConfig, bundle.units,
            *trainingFaction, bundle.factionModifiers, differentTypeNext);
        expectUnchanged(
            differentTypeResult,
            autochess::core::CommandErrorCode::NotMergeable,
            differentTypePlayer,
            differentTypeBefore,
            differentTypeNext,
            3,
            "MergeService rejects different unit types");

        // 此代码段验证不同单位等级不能合成。
        auto differentLevelPlayer = makePlayer(1);
        differentLevelPlayer.activeUnits[0].identity.level = 2;
        const auto differentLevelBefore = differentLevelPlayer;
        autochess::core::OwnedUnitId differentLevelNext = 3;
        const auto differentLevelResult = autochess::core::MergeService::merge(
            differentLevelPlayer, 1, 2, bundle.gameConfig, bundle.units,
            *trainingFaction, bundle.factionModifiers, differentLevelNext);
        expectUnchanged(
            differentLevelResult,
            autochess::core::CommandErrorCode::NotMergeable,
            differentLevelPlayer,
            differentLevelBefore,
            differentLevelNext,
            3,
            "MergeService rejects different unit levels");

        // 此代码段验证达到配置最高等级的单位不能继续合成。
        auto maxLevelPlayer = makePlayer(3);
        const auto maxLevelBefore = maxLevelPlayer;
        autochess::core::OwnedUnitId maxLevelNext = 3;
        const auto maxLevelResult = autochess::core::MergeService::merge(
            maxLevelPlayer, 1, 2, bundle.gameConfig, bundle.units,
            *trainingFaction, bundle.factionModifiers, maxLevelNext);
        expectUnchanged(
            maxLevelResult,
            autochess::core::CommandErrorCode::MaxLevelReached,
            maxLevelPlayer,
            maxLevelBefore,
            maxLevelNext,
            3,
            "MergeService rejects units at the maximum level");

        // 此代码段验证小于二级的最高等级配置会被明确拒绝。
        auto invalidLevelConfig = bundle.gameConfig;
        invalidLevelConfig.maxUnitLevel = 1;
        auto invalidLevelPlayer = makePlayer(1);
        const auto invalidLevelBefore = invalidLevelPlayer;
        autochess::core::OwnedUnitId invalidLevelNext = 3;
        const auto invalidLevelResult = autochess::core::MergeService::merge(
            invalidLevelPlayer, 1, 2, invalidLevelConfig, bundle.units,
            *trainingFaction, bundle.factionModifiers, invalidLevelNext);
        expectUnchanged(
            invalidLevelResult,
            autochess::core::CommandErrorCode::InvalidConfiguration,
            invalidLevelPlayer,
            invalidLevelBefore,
            invalidLevelNext,
            3,
            "MergeService rejects an invalid maximum level configuration");

        // 此代码段验证活动单位缺少对应配置定义时不会被合成。
        auto missingDefinitionPlayer = makePlayer(1);
        const auto missingDefinitionBefore = missingDefinitionPlayer;
        autochess::core::OwnedUnitId missingDefinitionNext = 3;
        const std::vector<autochess::core::UnitDefinition> noUnits;
        const auto missingDefinitionResult =
            autochess::core::MergeService::merge(
                missingDefinitionPlayer, 1, 2, bundle.gameConfig, noUnits,
                *trainingFaction, bundle.factionModifiers,
                missingDefinitionNext);
        expectUnchanged(
            missingDefinitionResult,
            autochess::core::CommandErrorCode::InvalidConfiguration,
            missingDefinitionPlayer,
            missingDefinitionBefore,
            missingDefinitionNext,
            3,
            "MergeService rejects a missing unit definition");

        // 此代码段验证传入分队与玩家分队不一致时不会计算合成价格。
        auto mismatchedFaction = *trainingFaction;
        mismatchedFaction.id = "other_team";
        auto mismatchedFactionPlayer = makePlayer(1);
        const auto mismatchedFactionBefore = mismatchedFactionPlayer;
        autochess::core::OwnedUnitId mismatchedFactionNext = 3;
        const auto mismatchedFactionResult =
            autochess::core::MergeService::merge(
                mismatchedFactionPlayer, 1, 2, bundle.gameConfig,
                bundle.units, mismatchedFaction, bundle.factionModifiers,
                mismatchedFactionNext);
        expectUnchanged(
            mismatchedFactionResult,
            autochess::core::CommandErrorCode::InconsistentState,
            mismatchedFactionPlayer,
            mismatchedFactionBefore,
            mismatchedFactionNext,
            3,
            "MergeService rejects a mismatched faction");

        // 此代码段验证无效的零值下一持久 ID 不会被消耗。
        auto zeroNextPlayer = makePlayer(1);
        const auto zeroNextBefore = zeroNextPlayer;
        autochess::core::OwnedUnitId zeroNextId =
            autochess::core::InvalidOwnedUnitId;
        const auto zeroNextResult = autochess::core::MergeService::merge(
            zeroNextPlayer, 1, 2, bundle.gameConfig, bundle.units,
            *trainingFaction, bundle.factionModifiers, zeroNextId);
        expectUnchanged(
            zeroNextResult,
            autochess::core::CommandErrorCode::InconsistentState,
            zeroNextPlayer,
            zeroNextBefore,
            zeroNextId,
            autochess::core::InvalidOwnedUnitId,
            "MergeService rejects an invalid zero next ID");

        // 此代码段验证下一持久 ID 不能与活动单位 ID 重复。
        auto activeDuplicatePlayer = makePlayer(1);
        const auto activeDuplicateBefore = activeDuplicatePlayer;
        autochess::core::OwnedUnitId activeDuplicateNext = 1;
        const auto activeDuplicateResult =
            autochess::core::MergeService::merge(
                activeDuplicatePlayer, 1, 2, bundle.gameConfig,
                bundle.units, *trainingFaction, bundle.factionModifiers,
                activeDuplicateNext);
        expectUnchanged(
            activeDuplicateResult,
            autochess::core::CommandErrorCode::InconsistentState,
            activeDuplicatePlayer,
            activeDuplicateBefore,
            activeDuplicateNext,
            1,
            "MergeService rejects a next ID used by an active unit");

        // 此代码段验证下一持久 ID 不能与死亡单位 ID 重复。
        auto deadDuplicatePlayer = makePlayer(1);
        deadDuplicatePlayer.deadUnits.push_back(
            autochess::core::OwnedUnit{
                3,
                {"training_guard", 1},
                autochess::core::MapSide::A});
        const auto deadDuplicateBefore = deadDuplicatePlayer;
        autochess::core::OwnedUnitId deadDuplicateNext = 3;
        const auto deadDuplicateResult =
            autochess::core::MergeService::merge(
                deadDuplicatePlayer, 1, 2, bundle.gameConfig,
                bundle.units, *trainingFaction, bundle.factionModifiers,
                deadDuplicateNext);
        expectUnchanged(
            deadDuplicateResult,
            autochess::core::CommandErrorCode::InconsistentState,
            deadDuplicatePlayer,
            deadDuplicateBefore,
            deadDuplicateNext,
            3,
            "MergeService rejects a next ID used by a dead unit");

        // 此代码段验证最大整数持久 ID 不会因递增而溢出。
        auto maximumNextPlayer = makePlayer(1);
        const auto maximumNextBefore = maximumNextPlayer;
        autochess::core::OwnedUnitId maximumNextId =
            std::numeric_limits<autochess::core::OwnedUnitId>::max();
        const auto maximumNextResult = autochess::core::MergeService::merge(
            maximumNextPlayer, 1, 2, bundle.gameConfig, bundle.units,
            *trainingFaction, bundle.factionModifiers, maximumNextId);
        expectUnchanged(
            maximumNextResult,
            autochess::core::CommandErrorCode::InconsistentState,
            maximumNextPlayer,
            maximumNextBefore,
            maximumNextId,
            std::numeric_limits<autochess::core::OwnedUnitId>::max(),
            "MergeService rejects the maximum next ID");

        // 此代码段验证初始玩家状态不一致时合成入口保持原子失败。
        auto invalidStatePlayer = makePlayer(1);
        invalidStatePlayer.reserveSlots[2] = 1;
        const auto invalidStateBefore = invalidStatePlayer;
        autochess::core::OwnedUnitId invalidStateNext = 3;
        const auto invalidStateResult = autochess::core::MergeService::merge(
            invalidStatePlayer, 1, 2, bundle.gameConfig, bundle.units,
            *trainingFaction, bundle.factionModifiers, invalidStateNext);
        expectUnchanged(
            invalidStateResult,
            autochess::core::CommandErrorCode::InconsistentState,
            invalidStatePlayer,
            invalidStateBefore,
            invalidStateNext,
            3,
            "MergeService rejects an invalid player state atomically");

        // 此代码段验证超出零到一范围的合成返还比例会被拒绝。
        auto invalidRatioConfig = bundle.gameConfig;
        invalidRatioConfig.mergeRefundRatio = 1.5;
        auto invalidRatioPlayer = makePlayer(1);
        const auto invalidRatioBefore = invalidRatioPlayer;
        autochess::core::OwnedUnitId invalidRatioNext = 3;
        const auto invalidRatioResult = autochess::core::MergeService::merge(
            invalidRatioPlayer, 1, 2, invalidRatioConfig, bundle.units,
            *trainingFaction, bundle.factionModifiers, invalidRatioNext);
        expectUnchanged(
            invalidRatioResult,
            autochess::core::CommandErrorCode::InvalidConfiguration,
            invalidRatioPlayer,
            invalidRatioBefore,
            invalidRatioNext,
            3,
            "MergeService rejects an invalid merge refund ratio");

        // 此代码段验证无效的单位一级价格不会产生合成返还。
        auto invalidPriceUnits = bundle.units;
        invalidPriceUnits.front().price = 0;
        auto invalidPricePlayer = makePlayer(1);
        const auto invalidPriceBefore = invalidPricePlayer;
        autochess::core::OwnedUnitId invalidPriceNext = 3;
        const auto invalidPriceResult = autochess::core::MergeService::merge(
            invalidPricePlayer, 1, 2, bundle.gameConfig, invalidPriceUnits,
            *trainingFaction, bundle.factionModifiers, invalidPriceNext);
        expectUnchanged(
            invalidPriceResult,
            autochess::core::CommandErrorCode::InvalidConfiguration,
            invalidPricePlayer,
            invalidPriceBefore,
            invalidPriceNext,
            3,
            "MergeService rejects an invalid unit price");

        // 此代码段验证合成返还导致金币整数溢出时状态保持不变。
        auto goldOverflowPlayer = makePlayer(1);
        goldOverflowPlayer.gold = std::numeric_limits<int>::max();
        const auto goldOverflowBefore = goldOverflowPlayer;
        autochess::core::OwnedUnitId goldOverflowNext = 3;
        const auto goldOverflowResult = autochess::core::MergeService::merge(
            goldOverflowPlayer, 1, 2, bundle.gameConfig, bundle.units,
            *trainingFaction, bundle.factionModifiers, goldOverflowNext);
        expectUnchanged(
            goldOverflowResult,
            autochess::core::CommandErrorCode::InconsistentState,
            goldOverflowPlayer,
            goldOverflowBefore,
            goldOverflowNext,
            3,
            "MergeService rejects a gold overflow atomically");

        // 此返回值汇总全部合成成功与失败案例的断言结果。
        return runner.failureCount();
    }

    // 此函数验证活动单位出售、死亡转移和死亡单位复活的完整规则。
    int runRosterServiceTests()
    {
        TestRunner runner;
        autochess::core::ConfigBundle bundle;
        autochess::core::ConfigError loadError;
        const bool loaded = autochess::core::ConfigBundleLoader::load(
            AUTOCHESS_DATA_DIR,
            bundle,
            loadError);
        runner.check(
            loaded,
            "RosterService loads the formal configuration bundle");
        if (!loaded)
        {
            return runner.failureCount();
        }

        const autochess::core::FactionDefinition* trainingFaction = nullptr;
        const autochess::core::UnitDefinition* trainingUnit = nullptr;
        for (const autochess::core::FactionDefinition& faction :
             bundle.factions)
        {
            if (faction.id == "training_team")
            {
                trainingFaction = &faction;
                break;
            }
        }
        for (const autochess::core::UnitDefinition& unit : bundle.units)
        {
            if (unit.id == "training_guard")
            {
                trainingUnit = &unit;
                break;
            }
        }

        runner.check(
            trainingFaction != nullptr && trainingUnit != nullptr,
            "RosterService finds the training faction and unit");
        if (trainingFaction == nullptr || trainingUnit == nullptr)
        {
            return runner.failureCount();
        }

        const auto makeActivePlayer = [&](const int level, const int gold)
        {
            auto player = autochess::core::PlayerStateService::createInitial(
                autochess::core::MapSide::A,
                bundle.gameConfig,
                *trainingFaction);
            player.gold = gold;
            player.activeUnits.push_back(
                autochess::core::OwnedUnit{
                    1,
                    autochess::core::UnitIdentity{
                        trainingUnit->id, level},
                    autochess::core::MapSide::A});
            player.reserveSlots[0] = 1;
            return player;
        };

        const auto makeDeadPlayer = [&](const int level, const int gold)
        {
            auto player = autochess::core::PlayerStateService::createInitial(
                autochess::core::MapSide::A,
                bundle.gameConfig,
                *trainingFaction);
            player.gold = gold;
            player.deadUnits.push_back(
                autochess::core::OwnedUnit{
                    1,
                    autochess::core::UnitIdentity{
                        trainingUnit->id, level},
                    autochess::core::MapSide::A});
            return player;
        };

        const auto expectUnchanged = [&runner](
            const autochess::core::CommandResult& result,
            const autochess::core::CommandErrorCode expectedCode,
            const autochess::core::PlayerState& actual,
            const autochess::core::PlayerState& before,
            const std::string& testName)
        {
            runner.check(
                !result.success
                    && result.errorCode == expectedCode
                    && !result.message.empty()
                    && playersAreEqual(actual, before),
                testName);
        };

        std::string validationError;

        // 此代码段验证备用区一级单位出售后被彻底移除并返还两个金币。
        auto reserveSellPlayer = makeActivePlayer(1, 10);
        const auto reserveSellResult = autochess::core::RosterService::sell(
            reserveSellPlayer,
            1,
            bundle.gameConfig,
            bundle.units,
            *trainingFaction,
            bundle.factionModifiers);
        runner.check(
            reserveSellResult.success
                && reserveSellResult.errorCode
                    == autochess::core::CommandErrorCode::None
                && reserveSellResult.message.find("2") != std::string::npos
                && reserveSellPlayer.gold == 12
                && reserveSellPlayer.activeUnits.empty()
                && reserveSellPlayer.deadUnits.empty()
                && !reserveSellPlayer.reserveSlots[0].has_value()
                && autochess::core::PlayerStateService::validate(
                    reserveSellPlayer, validationError),
            "RosterService sells a reserve level-one unit for two gold");

        // 此代码段验证三级部署单位仍按一级价格出售并释放部署格。
        auto deployedSellPlayer = makeActivePlayer(3, 10);
        deployedSellPlayer.reserveSlots[0].reset();
        deployedSellPlayer.deployments.emplace(
            autochess::core::GridPosition{1, 2}, 1);
        const auto deployedSellResult = autochess::core::RosterService::sell(
            deployedSellPlayer,
            1,
            bundle.gameConfig,
            bundle.units,
            *trainingFaction,
            bundle.factionModifiers);
        runner.check(
            deployedSellResult.success
                && deployedSellPlayer.gold == 12
                && deployedSellPlayer.activeUnits.empty()
                && deployedSellPlayer.deployments.empty()
                && autochess::core::PlayerStateService::validate(
                    deployedSellPlayer, validationError),
            "RosterService sells a deployed level-three unit at level-one value");

        // 此代码段验证零出售比例允许成功出售且不增加金币。
        auto freeSellConfig = bundle.gameConfig;
        freeSellConfig.sellRatio = 0.0;
        auto freeSellPlayer = makeActivePlayer(2, 10);
        const auto freeSellResult = autochess::core::RosterService::sell(
            freeSellPlayer,
            1,
            freeSellConfig,
            bundle.units,
            *trainingFaction,
            bundle.factionModifiers);
        runner.check(
            freeSellResult.success
                && freeSellPlayer.gold == 10
                && freeSellPlayer.activeUnits.empty()
                && autochess::core::PlayerStateService::validate(
                    freeSellPlayer, validationError),
            "RosterService permits a zero-ratio sale");

        // 此代码段验证死亡单位不能通过出售入口移除。
        auto deadSellPlayer = makeDeadPlayer(1, 10);
        const auto deadSellBefore = deadSellPlayer;
        const auto deadSellResult = autochess::core::RosterService::sell(
            deadSellPlayer,
            1,
            bundle.gameConfig,
            bundle.units,
            *trainingFaction,
            bundle.factionModifiers);
        expectUnchanged(
            deadSellResult,
            autochess::core::CommandErrorCode::UnitAlreadyDead,
            deadSellPlayer,
            deadSellBefore,
            "RosterService rejects selling a dead unit");

        // 此代码段验证不存在的持久 ID 不能出售。
        auto missingSellPlayer = makeActivePlayer(1, 10);
        const auto missingSellBefore = missingSellPlayer;
        const auto missingSellResult = autochess::core::RosterService::sell(
            missingSellPlayer,
            99,
            bundle.gameConfig,
            bundle.units,
            *trainingFaction,
            bundle.factionModifiers);
        expectUnchanged(
            missingSellResult,
            autochess::core::CommandErrorCode::UnitNotFound,
            missingSellPlayer,
            missingSellBefore,
            "RosterService rejects selling a missing unit");

        // 此代码段验证缺少单位定义时出售保持原子失败。
        auto missingSellDefinitionPlayer = makeActivePlayer(1, 10);
        const auto missingSellDefinitionBefore =
            missingSellDefinitionPlayer;
        const std::vector<autochess::core::UnitDefinition> noUnits;
        const auto missingSellDefinitionResult =
            autochess::core::RosterService::sell(
                missingSellDefinitionPlayer,
                1,
                bundle.gameConfig,
                noUnits,
                *trainingFaction,
                bundle.factionModifiers);
        expectUnchanged(
            missingSellDefinitionResult,
            autochess::core::CommandErrorCode::InvalidConfiguration,
            missingSellDefinitionPlayer,
            missingSellDefinitionBefore,
            "RosterService rejects selling a unit without a definition");

        // 此代码段验证出售价格分队必须与玩家分队一致。
        auto otherFaction = *trainingFaction;
        otherFaction.id = "other_team";
        auto mismatchedSellPlayer = makeActivePlayer(1, 10);
        const auto mismatchedSellBefore = mismatchedSellPlayer;
        const auto mismatchedSellResult = autochess::core::RosterService::sell(
            mismatchedSellPlayer,
            1,
            bundle.gameConfig,
            bundle.units,
            otherFaction,
            bundle.factionModifiers);
        expectUnchanged(
            mismatchedSellResult,
            autochess::core::CommandErrorCode::InconsistentState,
            mismatchedSellPlayer,
            mismatchedSellBefore,
            "RosterService rejects a mismatched sale faction");

        // 此代码段验证非法出售比例不会移除单位。
        auto invalidSellConfig = bundle.gameConfig;
        invalidSellConfig.sellRatio = 1.5;
        auto invalidSellRatioPlayer = makeActivePlayer(1, 10);
        const auto invalidSellRatioBefore = invalidSellRatioPlayer;
        const auto invalidSellRatioResult =
            autochess::core::RosterService::sell(
                invalidSellRatioPlayer,
                1,
                invalidSellConfig,
                bundle.units,
                *trainingFaction,
                bundle.factionModifiers);
        expectUnchanged(
            invalidSellRatioResult,
            autochess::core::CommandErrorCode::InvalidConfiguration,
            invalidSellRatioPlayer,
            invalidSellRatioBefore,
            "RosterService rejects an invalid sale ratio");

        // 此代码段验证出售返还导致金币溢出时拒绝提交。
        auto overflowSellPlayer = makeActivePlayer(
            1, std::numeric_limits<int>::max());
        const auto overflowSellBefore = overflowSellPlayer;
        const auto overflowSellResult = autochess::core::RosterService::sell(
            overflowSellPlayer,
            1,
            bundle.gameConfig,
            bundle.units,
            *trainingFaction,
            bundle.factionModifiers);
        expectUnchanged(
            overflowSellResult,
            autochess::core::CommandErrorCode::InconsistentState,
            overflowSellPlayer,
            overflowSellBefore,
            "RosterService rejects sale gold overflow atomically");

        // 此代码段验证出售入口先拒绝违反位置不变量的玩家状态。
        auto invalidSellStatePlayer = makeActivePlayer(1, 10);
        invalidSellStatePlayer.reserveSlots[1] = 1;
        const auto invalidSellStateBefore = invalidSellStatePlayer;
        const auto invalidSellStateResult =
            autochess::core::RosterService::sell(
                invalidSellStatePlayer,
                1,
                bundle.gameConfig,
                bundle.units,
                *trainingFaction,
                bundle.factionModifiers);
        expectUnchanged(
            invalidSellStateResult,
            autochess::core::CommandErrorCode::InconsistentState,
            invalidSellStatePlayer,
            invalidSellStateBefore,
            "RosterService rejects an invalid state before selling");

        // 此代码段验证备用单位死亡后完整身份进入死亡列表且金币不变。
        auto reserveDeathPlayer = makeActivePlayer(2, 10);
        const int guardBeforeDeath = reserveDeathPlayer.guardValue;
        const auto reserveDeathResult =
            autochess::core::RosterService::markDead(
                reserveDeathPlayer, 1);
        runner.check(
            reserveDeathResult.success
                && reserveDeathResult.errorCode
                    == autochess::core::CommandErrorCode::None
                && !reserveDeathResult.message.empty()
                && reserveDeathPlayer.gold == 10
                && reserveDeathPlayer.guardValue == guardBeforeDeath
                && reserveDeathPlayer.activeUnits.empty()
                && reserveDeathPlayer.deadUnits.size() == 1
                && reserveDeathPlayer.deadUnits.front().id == 1
                && reserveDeathPlayer.deadUnits.front().identity.level == 2
                && !reserveDeathPlayer.reserveSlots[0].has_value()
                && autochess::core::PlayerStateService::validate(
                    reserveDeathPlayer, validationError),
            "RosterService moves a reserve unit to the dead list");

        // 此代码段验证部署单位死亡后对应部署格被释放。
        auto deployedDeathPlayer = makeActivePlayer(1, 10);
        deployedDeathPlayer.reserveSlots[0].reset();
        deployedDeathPlayer.deployments.emplace(
            autochess::core::GridPosition{1, 2}, 1);
        const auto deployedDeathResult =
            autochess::core::RosterService::markDead(
                deployedDeathPlayer, 1);
        runner.check(
            deployedDeathResult.success
                && deployedDeathPlayer.activeUnits.empty()
                && deployedDeathPlayer.deadUnits.size() == 1
                && deployedDeathPlayer.deployments.empty()
                && autochess::core::PlayerStateService::validate(
                    deployedDeathPlayer, validationError),
            "RosterService releases a deployment when a unit dies");

        // 此代码段验证同一单位不能重复进入死亡列表。
        const auto repeatedDeathBefore = reserveDeathPlayer;
        const auto repeatedDeathResult =
            autochess::core::RosterService::markDead(
                reserveDeathPlayer, 1);
        expectUnchanged(
            repeatedDeathResult,
            autochess::core::CommandErrorCode::UnitAlreadyDead,
            reserveDeathPlayer,
            repeatedDeathBefore,
            "RosterService rejects marking an already dead unit");

        // 此代码段验证不存在的单位不能转入死亡列表。
        auto missingDeathPlayer = makeActivePlayer(1, 10);
        const auto missingDeathBefore = missingDeathPlayer;
        const auto missingDeathResult =
            autochess::core::RosterService::markDead(
                missingDeathPlayer, 99);
        expectUnchanged(
            missingDeathResult,
            autochess::core::CommandErrorCode::UnitNotFound,
            missingDeathPlayer,
            missingDeathBefore,
            "RosterService rejects marking a missing unit dead");

        // 此代码段验证死亡入口不会修改无效的初始玩家状态。
        auto invalidDeathStatePlayer = makeActivePlayer(1, 10);
        invalidDeathStatePlayer.deployments.emplace(
            autochess::core::GridPosition{1, 2}, 1);
        const auto invalidDeathStateBefore = invalidDeathStatePlayer;
        const auto invalidDeathStateResult =
            autochess::core::RosterService::markDead(
                invalidDeathStatePlayer, 1);
        expectUnchanged(
            invalidDeathStateResult,
            autochess::core::CommandErrorCode::InconsistentState,
            invalidDeathStatePlayer,
            invalidDeathStateBefore,
            "RosterService rejects an invalid state before death transfer");

        // 此代码段验证二级死亡单位以同一 ID 复活并扣除两个金币。
        auto revivePlayer = makeDeadPlayer(2, 10);
        const auto reviveResult = autochess::core::RosterService::revive(
            revivePlayer,
            1,
            bundle.gameConfig,
            bundle.units,
            *trainingFaction,
            bundle.factionModifiers);
        runner.check(
            reviveResult.success
                && reviveResult.errorCode
                    == autochess::core::CommandErrorCode::None
                && reviveResult.message.find("2") != std::string::npos
                && revivePlayer.gold == 8
                && revivePlayer.deadUnits.empty()
                && revivePlayer.activeUnits.size() == 1
                && revivePlayer.activeUnits.front().id == 1
                && revivePlayer.activeUnits.front().identity.level == 2
                && revivePlayer.reserveSlots[0].has_value()
                && revivePlayer.reserveSlots[0].value() == 1
                && revivePlayer.deployments.empty()
                && autochess::core::PlayerStateService::validate(
                    revivePlayer, validationError),
            "RosterService revives a level-two unit with the same ID");

        // 此代码段验证复活单位进入编号最小的空闲备用区槽位。
        auto firstEmptyPlayer = makeDeadPlayer(3, 10);
        firstEmptyPlayer.activeUnits.push_back(
            autochess::core::OwnedUnit{
                2,
                autochess::core::UnitIdentity{trainingUnit->id, 1},
                autochess::core::MapSide::A});
        firstEmptyPlayer.reserveSlots[0] = 2;
        const auto firstEmptyResult =
            autochess::core::RosterService::revive(
                firstEmptyPlayer,
                1,
                bundle.gameConfig,
                bundle.units,
                *trainingFaction,
                bundle.factionModifiers);
        runner.check(
            firstEmptyResult.success
                && firstEmptyPlayer.reserveSlots[1].has_value()
                && firstEmptyPlayer.reserveSlots[1].value() == 1
                && firstEmptyPlayer.activeUnits.size() == 2
                && firstEmptyPlayer.gold == 8
                && autochess::core::PlayerStateService::validate(
                    firstEmptyPlayer, validationError),
            "RosterService revives into the first empty reserve slot");

        // 此代码段验证金币不足时死亡单位保持不变。
        auto poorRevivePlayer = makeDeadPlayer(1, 1);
        const auto poorReviveBefore = poorRevivePlayer;
        const auto poorReviveResult = autochess::core::RosterService::revive(
            poorRevivePlayer,
            1,
            bundle.gameConfig,
            bundle.units,
            *trainingFaction,
            bundle.factionModifiers);
        expectUnchanged(
            poorReviveResult,
            autochess::core::CommandErrorCode::InsufficientGold,
            poorRevivePlayer,
            poorReviveBefore,
            "RosterService rejects revival with insufficient gold");

        // 此代码段填满活动单位容量并验证死亡单位不能复活。
        auto fullRevivePlayer = makeDeadPlayer(1, 10);
        for (std::size_t slot = 0;
             slot < fullRevivePlayer.reserveSlots.size();
             ++slot)
        {
            const auto id = static_cast<autochess::core::OwnedUnitId>(
                slot + 2);
            fullRevivePlayer.activeUnits.push_back(
                autochess::core::OwnedUnit{
                    id,
                    autochess::core::UnitIdentity{trainingUnit->id, 1},
                    autochess::core::MapSide::A});
            fullRevivePlayer.reserveSlots[slot] = id;
        }
        const auto fullReviveBefore = fullRevivePlayer;
        const auto fullReviveResult = autochess::core::RosterService::revive(
            fullRevivePlayer,
            1,
            bundle.gameConfig,
            bundle.units,
            *trainingFaction,
            bundle.factionModifiers);
        expectUnchanged(
            fullReviveResult,
            autochess::core::CommandErrorCode::RosterFull,
            fullRevivePlayer,
            fullReviveBefore,
            "RosterService enforces the active roster limit on revival");

        // 此代码段验证活动单位不能通过复活入口重复加入活动列表。
        auto activeRevivePlayer = makeActivePlayer(1, 10);
        const auto activeReviveBefore = activeRevivePlayer;
        const auto activeReviveResult =
            autochess::core::RosterService::revive(
                activeRevivePlayer,
                1,
                bundle.gameConfig,
                bundle.units,
                *trainingFaction,
                bundle.factionModifiers);
        expectUnchanged(
            activeReviveResult,
            autochess::core::CommandErrorCode::UnitAlreadyActive,
            activeRevivePlayer,
            activeReviveBefore,
            "RosterService rejects reviving an active unit");

        // 此代码段验证不存在的持久 ID 不能复活。
        auto missingRevivePlayer = makeDeadPlayer(1, 10);
        const auto missingReviveBefore = missingRevivePlayer;
        const auto missingReviveResult =
            autochess::core::RosterService::revive(
                missingRevivePlayer,
                99,
                bundle.gameConfig,
                bundle.units,
                *trainingFaction,
                bundle.factionModifiers);
        expectUnchanged(
            missingReviveResult,
            autochess::core::CommandErrorCode::UnitNotFound,
            missingRevivePlayer,
            missingReviveBefore,
            "RosterService rejects reviving a missing unit");

        // 此代码段验证缺少单位定义时复活保持原子失败。
        auto missingReviveDefinitionPlayer = makeDeadPlayer(1, 10);
        const auto missingReviveDefinitionBefore =
            missingReviveDefinitionPlayer;
        const auto missingReviveDefinitionResult =
            autochess::core::RosterService::revive(
                missingReviveDefinitionPlayer,
                1,
                bundle.gameConfig,
                noUnits,
                *trainingFaction,
                bundle.factionModifiers);
        expectUnchanged(
            missingReviveDefinitionResult,
            autochess::core::CommandErrorCode::InvalidConfiguration,
            missingReviveDefinitionPlayer,
            missingReviveDefinitionBefore,
            "RosterService rejects reviving a unit without a definition");

        // 此代码段验证复活价格分队必须与玩家分队一致。
        auto mismatchedRevivePlayer = makeDeadPlayer(1, 10);
        const auto mismatchedReviveBefore = mismatchedRevivePlayer;
        const auto mismatchedReviveResult =
            autochess::core::RosterService::revive(
                mismatchedRevivePlayer,
                1,
                bundle.gameConfig,
                bundle.units,
                otherFaction,
                bundle.factionModifiers);
        expectUnchanged(
            mismatchedReviveResult,
            autochess::core::CommandErrorCode::InconsistentState,
            mismatchedRevivePlayer,
            mismatchedReviveBefore,
            "RosterService rejects a mismatched revival faction");

        // 此代码段验证非法复活比例不会移动死亡单位。
        auto invalidReviveConfig = bundle.gameConfig;
        invalidReviveConfig.reviveRatio = -0.1;
        auto invalidReviveRatioPlayer = makeDeadPlayer(1, 10);
        const auto invalidReviveRatioBefore = invalidReviveRatioPlayer;
        const auto invalidReviveRatioResult =
            autochess::core::RosterService::revive(
                invalidReviveRatioPlayer,
                1,
                invalidReviveConfig,
                bundle.units,
                *trainingFaction,
                bundle.factionModifiers);
        expectUnchanged(
            invalidReviveRatioResult,
            autochess::core::CommandErrorCode::InvalidConfiguration,
            invalidReviveRatioPlayer,
            invalidReviveRatioBefore,
            "RosterService rejects an invalid revival ratio");

        // 此代码段验证零复活比例允许免费复活。
        auto freeReviveConfig = bundle.gameConfig;
        freeReviveConfig.reviveRatio = 0.0;
        auto freeRevivePlayer = makeDeadPlayer(1, 0);
        const auto freeReviveResult = autochess::core::RosterService::revive(
            freeRevivePlayer,
            1,
            freeReviveConfig,
            bundle.units,
            *trainingFaction,
            bundle.factionModifiers);
        runner.check(
            freeReviveResult.success
                && freeRevivePlayer.gold == 0
                && freeRevivePlayer.deadUnits.empty()
                && freeRevivePlayer.activeUnits.size() == 1
                && autochess::core::PlayerStateService::validate(
                    freeRevivePlayer, validationError),
            "RosterService permits a zero-cost revival");

        // 此代码段验证复活入口不会修改无效的初始玩家状态。
        auto invalidReviveStatePlayer = makeDeadPlayer(1, 10);
        invalidReviveStatePlayer.reserveSlots[0] = 1;
        const auto invalidReviveStateBefore = invalidReviveStatePlayer;
        const auto invalidReviveStateResult =
            autochess::core::RosterService::revive(
                invalidReviveStatePlayer,
                1,
                bundle.gameConfig,
                bundle.units,
                *trainingFaction,
                bundle.factionModifiers);
        expectUnchanged(
            invalidReviveStateResult,
            autochess::core::CommandErrorCode::InconsistentState,
            invalidReviveStatePlayer,
            invalidReviveStateBefore,
            "RosterService rejects an invalid state before revival");

        // 此代码段串联死亡与复活，确认死亡释放容量且复活保留身份。
        auto lifecyclePlayer = makeActivePlayer(3, 10);
        const auto lifecycleDeathResult =
            autochess::core::RosterService::markDead(
                lifecyclePlayer, 1);
        const auto lifecycleReviveResult =
            autochess::core::RosterService::revive(
                lifecyclePlayer,
                1,
                bundle.gameConfig,
                bundle.units,
                *trainingFaction,
                bundle.factionModifiers);
        runner.check(
            lifecycleDeathResult.success
                && lifecycleReviveResult.success
                && lifecyclePlayer.activeUnits.size() == 1
                && lifecyclePlayer.deadUnits.empty()
                && lifecyclePlayer.activeUnits.front().id == 1
                && lifecyclePlayer.activeUnits.front().identity.level == 3
                && lifecyclePlayer.gold == 8
                && autochess::core::PlayerStateService::validate(
                    lifecyclePlayer, validationError),
            "RosterService death and revival preserve persistent identity");

        return runner.failureCount();
    }

    struct EconomyScenarioRun
    {
        bool completed = false;
        autochess::core::PlayerState player;
        autochess::core::ShopState shop;
        autochess::core::OwnedUnitId nextOwnedUnitId = 1;
        std::vector<autochess::core::CommandResult> results;
        std::vector<autochess::core::PlayerState> playerStates;
        std::vector<autochess::core::ShopState> shopStates;
        std::vector<int> goldLog;
        std::vector<autochess::core::OwnedUnitId> nextIdLog;
    };

    bool recordEconomyScenarioStep(
        EconomyScenarioRun& run,
        const autochess::core::CommandResult& result)
    {
        run.results.push_back(result);
        run.playerStates.push_back(run.player);
        run.shopStates.push_back(run.shop);
        run.goldLog.push_back(run.player.gold);
        run.nextIdLog.push_back(run.nextOwnedUnitId);
        return result.success;
    }

    EconomyScenarioRun executeEconomyScenario(
        const autochess::core::ConfigBundle& bundle,
        const autochess::core::FactionDefinition& faction,
        const autochess::core::MapDefinition& map)
    {
        EconomyScenarioRun run;
        run.player = autochess::core::PlayerStateService::createInitial(
            autochess::core::MapSide::A,
            bundle.gameConfig,
            faction);
        run.playerStates.push_back(run.player);
        run.shopStates.push_back(run.shop);
        run.goldLog.push_back(run.player.gold);
        run.nextIdLog.push_back(run.nextOwnedUnitId);

        std::mt19937 randomEngine(bundle.gameConfig.randomSeed);

        if (!recordEconomyScenarioStep(
                run,
                autochess::core::ShopService::rebuild(
                    run.player,
                    run.shop,
                    bundle.gameConfig,
                    bundle.units,
                    faction,
                    bundle.factionModifiers,
                    randomEngine)))
        {
            return run;
        }

        if (!recordEconomyScenarioStep(
                run,
                autochess::core::ShopService::purchase(
                    run.player,
                    run.shop,
                    0,
                    bundle.units,
                    run.nextOwnedUnitId)))
        {
            return run;
        }

        if (!recordEconomyScenarioStep(
                run,
                autochess::core::ShopService::purchase(
                    run.player,
                    run.shop,
                    1,
                    bundle.units,
                    run.nextOwnedUnitId)))
        {
            return run;
        }

        if (!recordEconomyScenarioStep(
                run,
                autochess::core::ShopService::purchase(
                    run.player,
                    run.shop,
                    2,
                    bundle.units,
                    run.nextOwnedUnitId)))
        {
            return run;
        }

        if (!recordEconomyScenarioStep(
                run,
                autochess::core::MergeService::merge(
                    run.player,
                    1,
                    2,
                    bundle.gameConfig,
                    bundle.units,
                    faction,
                    bundle.factionModifiers,
                    run.nextOwnedUnitId)))
        {
            return run;
        }

        if (!recordEconomyScenarioStep(
                run,
                autochess::core::DeploymentService::moveToDeployment(
                    run.player,
                    map,
                    faction,
                    4,
                    autochess::core::GridPosition{1, 2})))
        {
            return run;
        }

        if (!recordEconomyScenarioStep(
                run,
                autochess::core::DeploymentService::moveToDeployment(
                    run.player,
                    map,
                    faction,
                    3,
                    autochess::core::GridPosition{1, 4})))
        {
            return run;
        }

        if (!recordEconomyScenarioStep(
                run,
                autochess::core::RosterService::sell(
                    run.player,
                    3,
                    bundle.gameConfig,
                    bundle.units,
                    faction,
                    bundle.factionModifiers)))
        {
            return run;
        }

        if (!recordEconomyScenarioStep(
                run,
                autochess::core::RosterService::markDead(
                    run.player,
                    4)))
        {
            return run;
        }

        if (!recordEconomyScenarioStep(
                run,
                autochess::core::RosterService::revive(
                    run.player,
                    4,
                    bundle.gameConfig,
                    bundle.units,
                    faction,
                    bundle.factionModifiers)))
        {
            return run;
        }

        if (!recordEconomyScenarioStep(
                run,
                autochess::core::ShopService::refresh(
                    run.player,
                    run.shop,
                    bundle.gameConfig,
                    bundle.units,
                    faction,
                    bundle.factionModifiers,
                    randomEngine)))
        {
            return run;
        }

        run.completed = true;
        return run;
    }

    bool economyScenarioRunsAreEqual(
        const EconomyScenarioRun& left,
        const EconomyScenarioRun& right)
    {
        if (left.completed != right.completed
            || left.nextOwnedUnitId != right.nextOwnedUnitId
            || left.goldLog != right.goldLog
            || left.nextIdLog != right.nextIdLog
            || left.results.size() != right.results.size()
            || left.playerStates.size() != right.playerStates.size()
            || left.shopStates.size() != right.shopStates.size())
        {
            return false;
        }

        for (std::size_t index = 0; index < left.results.size(); ++index)
        {
            if (left.results[index].success != right.results[index].success
                || left.results[index].errorCode
                    != right.results[index].errorCode
                || left.results[index].message
                    != right.results[index].message)
            {
                return false;
            }
        }

        for (std::size_t index = 0;
             index < left.playerStates.size();
             ++index)
        {
            if (!playersAreEqual(
                    left.playerStates[index], right.playerStates[index])
                || !shopsAreEqual(
                    left.shopStates[index], right.shopStates[index]))
            {
                return false;
            }
        }

        return playersAreEqual(left.player, right.player)
            && shopsAreEqual(left.shop, right.shop);
    }

    int runEconomyIntegrationScenarioTests()
    {
        TestRunner runner;
        autochess::core::ConfigBundle bundle;
        autochess::core::ConfigError loadError;
        const bool loaded = autochess::core::ConfigBundleLoader::load(
            AUTOCHESS_DATA_DIR,
            bundle,
            loadError);
        runner.check(
            loaded,
            "Economy integration loads the formal configuration bundle");
        if (!loaded)
        {
            return runner.failureCount();
        }

        const autochess::core::FactionDefinition* trainingFaction = nullptr;
        const autochess::core::UnitDefinition* trainingUnit = nullptr;
        const autochess::core::MapDefinition* trainingMap = nullptr;
        for (const autochess::core::FactionDefinition& faction :
             bundle.factions)
        {
            if (faction.id == "training_team")
            {
                trainingFaction = &faction;
                break;
            }
        }
        for (const autochess::core::UnitDefinition& unit : bundle.units)
        {
            if (unit.id == "training_guard")
            {
                trainingUnit = &unit;
                break;
            }
        }
        for (const autochess::core::MapDefinition& map : bundle.maps)
        {
            if (map.id == "map_01")
            {
                trainingMap = &map;
                break;
            }
        }

        const bool formalDataFound = trainingFaction != nullptr
            && trainingUnit != nullptr
            && trainingMap != nullptr
            && bundle.gameConfig.startingGold == 10
            && bundle.gameConfig.rosterCapacity == 8
            && bundle.gameConfig.shopSlots == 6
            && bundle.gameConfig.shopRefreshCost == 2
            && trainingUnit->price == 3;
        runner.check(
            formalDataFound,
            "Economy integration finds the formal faction, unit, map, and values");
        if (!formalDataFound)
        {
            return runner.failureCount();
        }

        // 此代码块用单一正式单位池保持经济场景的固定金币日志可重复。
        autochess::core::ConfigBundle economyBundle = bundle;
        economyBundle.units.clear();
        economyBundle.units.push_back(*trainingUnit);
        const EconomyScenarioRun firstRun = executeEconomyScenario(
            economyBundle, *trainingFaction, *trainingMap);
        bool everyCommandSucceeded = firstRun.completed
            && firstRun.results.size() == 11;
        for (const autochess::core::CommandResult& result : firstRun.results)
        {
            everyCommandSucceeded = everyCommandSucceeded
                && result.success
                && result.errorCode
                    == autochess::core::CommandErrorCode::None
                && !result.message.empty();
        }
        runner.check(
            everyCommandSucceeded,
            "Economy integration completes every command with a message");
        if (!firstRun.completed
            || firstRun.playerStates.size() != 12
            || firstRun.shopStates.size() != 12)
        {
            return runner.failureCount();
        }

        bool everyStateValid = true;
        for (const autochess::core::PlayerState& state :
             firstRun.playerStates)
        {
            std::string validationError;
            everyStateValid = everyStateValid
                && autochess::core::PlayerStateService::validate(
                    state, validationError);
        }
        runner.check(
            everyStateValid,
            "Economy integration keeps every intermediate player state valid");

        const auto allOffersMatchFormalUnit = [trainingUnit](
            const autochess::core::ShopState& shop)
        {
            if (shop.offers.size() != 6)
            {
                return false;
            }

            for (const std::optional<autochess::core::ShopOffer>& offer :
                 shop.offers)
            {
                if (!offer.has_value()
                    || offer->unitId != trainingUnit->id
                    || offer->displayedPrice != 3)
                {
                    return false;
                }
            }

            return true;
        };

        const auto& rebuiltState = firstRun.playerStates[1];
        runner.check(
            rebuiltState.gold == 10
                && rebuiltState.activeUnits.empty()
                && allOffersMatchFormalUnit(firstRun.shopStates[1]),
            "Economy integration rebuilds six formal offers without charging gold");

        const auto& firstPurchaseState = firstRun.playerStates[2];
        const auto& secondPurchaseState = firstRun.playerStates[3];
        const auto& thirdPurchaseState = firstRun.playerStates[4];
        runner.check(
            firstPurchaseState.gold == 7
                && firstPurchaseState.activeUnits.size() == 1
                && firstPurchaseState.reserveSlots[0] == 1
                && firstRun.nextIdLog[2] == 2
                && secondPurchaseState.gold == 4
                && secondPurchaseState.activeUnits.size() == 2
                && secondPurchaseState.reserveSlots[1] == 2
                && firstRun.nextIdLog[3] == 3
                && thirdPurchaseState.gold == 1
                && thirdPurchaseState.activeUnits.size() == 3
                && thirdPurchaseState.reserveSlots[2] == 3
                && firstRun.nextIdLog[4] == 4,
            "Economy integration purchases IDs one through three into reserve slots");

        const auto& mergedState = firstRun.playerStates[5];
        const autochess::core::OwnedUnit* mergedUnit =
            autochess::core::PlayerStateService::findActive(mergedState, 4);
        runner.check(
            mergedState.gold == 2
                && mergedState.activeUnits.size() == 2
                && autochess::core::PlayerStateService::findActive(
                    mergedState, 1) == nullptr
                && autochess::core::PlayerStateService::findActive(
                    mergedState, 2) == nullptr
                && mergedUnit != nullptr
                && mergedUnit->identity.unitId == trainingUnit->id
                && mergedUnit->identity.level == 2
                && mergedState.reserveSlots[1] == 4
                && mergedState.reserveSlots[2] == 3
                && firstRun.nextIdLog[5] == 5,
            "Economy integration merges IDs one and two into level-two ID four");

        const auto& firstDeploymentState = firstRun.playerStates[6];
        const auto firstDeployment =
            autochess::core::PlayerStateService::findDeploymentPosition(
                firstDeploymentState, 4);
        runner.check(
            firstDeploymentState.gold == 2
                && firstDeploymentState.deployments.size() == 1
                && firstDeployment.has_value()
                && firstDeployment.value()
                    == autochess::core::GridPosition{1, 2}
                && !firstDeploymentState.reserveSlots[1].has_value(),
            "Economy integration deploys level-two ID four to the upper start");

        const auto& secondDeploymentState = firstRun.playerStates[7];
        const auto secondDeployment =
            autochess::core::PlayerStateService::findDeploymentPosition(
                secondDeploymentState, 3);
        runner.check(
            secondDeploymentState.gold == 2
                && secondDeploymentState.deployments.size() == 2
                && secondDeployment.has_value()
                && secondDeployment.value()
                    == autochess::core::GridPosition{1, 4}
                && !secondDeploymentState.reserveSlots[2].has_value(),
            "Economy integration deploys ID three to the lower start");

        const auto& soldState = firstRun.playerStates[8];
        runner.check(
            soldState.gold == 4
                && soldState.activeUnits.size() == 1
                && autochess::core::PlayerStateService::findActive(
                    soldState, 3) == nullptr
                && soldState.deployments.size() == 1
                && soldState.deployments.find(
                    autochess::core::GridPosition{1, 4})
                    == soldState.deployments.end(),
            "Economy integration sells deployed ID three for two gold");

        const auto& deadState = firstRun.playerStates[9];
        const autochess::core::OwnedUnit* deadUnit =
            autochess::core::PlayerStateService::findDead(deadState, 4);
        runner.check(
            deadState.gold == 4
                && deadState.activeUnits.empty()
                && deadState.deadUnits.size() == 1
                && deadUnit != nullptr
                && deadUnit->identity.level == 2
                && deadState.deployments.empty(),
            "Economy integration moves deployed ID four to the dead list");

        const auto& revivedState = firstRun.playerStates[10];
        const autochess::core::OwnedUnit* revivedUnit =
            autochess::core::PlayerStateService::findActive(revivedState, 4);
        runner.check(
            revivedState.gold == 2
                && revivedState.activeUnits.size() == 1
                && revivedState.deadUnits.empty()
                && revivedUnit != nullptr
                && revivedUnit->identity.level == 2
                && revivedState.reserveSlots[0] == 4
                && revivedState.deployments.empty(),
            "Economy integration revives ID four into the first reserve slot");

        const auto& finalState = firstRun.playerStates[11];
        runner.check(
            finalState.gold == 0
                && finalState.activeUnits.size() == 1
                && finalState.deadUnits.empty()
                && finalState.reserveSlots[0] == 4
                && finalState.deployments.empty()
                && firstRun.nextOwnedUnitId == 5
                && allOffersMatchFormalUnit(firstRun.shopStates[11]),
            "Economy integration refreshes the shop and reaches the expected final state");

        const std::vector<int> expectedGoldLog = {
            10, 10, 7, 4, 1, 2, 2, 2, 4, 4, 2, 0};
        runner.check(
            firstRun.goldLog == expectedGoldLog,
            "Economy integration records the exact expected gold sequence");

        auto poorPurchasePlayer =
            autochess::core::PlayerStateService::createInitial(
                autochess::core::MapSide::A,
                bundle.gameConfig,
                *trainingFaction);
        poorPurchasePlayer.gold = 2;
        autochess::core::ShopState poorPurchaseShop;
        std::mt19937 poorPurchaseEngine(bundle.gameConfig.randomSeed);
        const auto poorPurchaseRebuild =
            autochess::core::ShopService::rebuild(
                poorPurchasePlayer,
                poorPurchaseShop,
                bundle.gameConfig,
                bundle.units,
                *trainingFaction,
                bundle.factionModifiers,
                poorPurchaseEngine);
        const auto poorPurchaseBefore = poorPurchasePlayer;
        const auto poorPurchaseShopBefore = poorPurchaseShop;
        autochess::core::OwnedUnitId poorPurchaseNextId = 1;
        const auto poorPurchaseResult =
            autochess::core::ShopService::purchase(
                poorPurchasePlayer,
                poorPurchaseShop,
                0,
                bundle.units,
                poorPurchaseNextId);
        runner.check(
            poorPurchaseRebuild.success
                && !poorPurchaseResult.success
                && poorPurchaseResult.errorCode
                    == autochess::core::CommandErrorCode::InsufficientGold
                && !poorPurchaseResult.message.empty()
                && playersAreEqual(
                    poorPurchasePlayer, poorPurchaseBefore)
                && shopsAreEqual(
                    poorPurchaseShop, poorPurchaseShopBefore)
                && poorPurchaseNextId == 1,
            "Economy integration rejects an unaffordable purchase atomically");

        auto poorRefreshPlayer =
            autochess::core::PlayerStateService::createInitial(
                autochess::core::MapSide::A,
                bundle.gameConfig,
                *trainingFaction);
        poorRefreshPlayer.gold = 1;
        autochess::core::ShopState poorRefreshShop;
        std::mt19937 poorRefreshEngine(bundle.gameConfig.randomSeed);
        const auto poorRefreshRebuild =
            autochess::core::ShopService::rebuild(
                poorRefreshPlayer,
                poorRefreshShop,
                bundle.gameConfig,
                bundle.units,
                *trainingFaction,
                bundle.factionModifiers,
                poorRefreshEngine);
        const auto poorRefreshBefore = poorRefreshPlayer;
        const auto poorRefreshShopBefore = poorRefreshShop;
        const auto poorRefreshEngineBefore = poorRefreshEngine;
        const auto poorRefreshResult = autochess::core::ShopService::refresh(
            poorRefreshPlayer,
            poorRefreshShop,
            bundle.gameConfig,
            bundle.units,
            *trainingFaction,
            bundle.factionModifiers,
            poorRefreshEngine);
        runner.check(
            poorRefreshRebuild.success
                && !poorRefreshResult.success
                && poorRefreshResult.errorCode
                    == autochess::core::CommandErrorCode::InsufficientGold
                && !poorRefreshResult.message.empty()
                && playersAreEqual(poorRefreshPlayer, poorRefreshBefore)
                && shopsAreEqual(poorRefreshShop, poorRefreshShopBefore)
                && poorRefreshEngine == poorRefreshEngineBefore,
            "Economy integration rejects an unaffordable refresh atomically");

        auto deploymentPlayer =
            autochess::core::PlayerStateService::createInitial(
                autochess::core::MapSide::A,
                bundle.gameConfig,
                *trainingFaction);
        autochess::core::ShopState deploymentShop;
        std::mt19937 deploymentEngine(bundle.gameConfig.randomSeed);
        autochess::core::OwnedUnitId deploymentNextId = 1;
        const auto deploymentRebuild = autochess::core::ShopService::rebuild(
            deploymentPlayer,
            deploymentShop,
            bundle.gameConfig,
            bundle.units,
            *trainingFaction,
            bundle.factionModifiers,
            deploymentEngine);
        const auto firstDeploymentPurchase =
            autochess::core::ShopService::purchase(
                deploymentPlayer,
                deploymentShop,
                0,
                bundle.units,
                deploymentNextId);
        const auto secondDeploymentPurchase =
            autochess::core::ShopService::purchase(
                deploymentPlayer,
                deploymentShop,
                1,
                bundle.units,
                deploymentNextId);
        const auto wrongSideBefore = deploymentPlayer;
        const auto wrongSideResult =
            autochess::core::DeploymentService::moveToDeployment(
                deploymentPlayer,
                *trainingMap,
                *trainingFaction,
                1,
                autochess::core::GridPosition{9, 2});
        runner.check(
            deploymentRebuild.success
                && firstDeploymentPurchase.success
                && secondDeploymentPurchase.success
                && !wrongSideResult.success
                && wrongSideResult.errorCode
                    == autochess::core::CommandErrorCode::InvalidTarget
                && !wrongSideResult.message.empty()
                && playersAreEqual(deploymentPlayer, wrongSideBefore),
            "Economy integration rejects the opposing deployment start atomically");

        const auto legalDeploymentResult =
            autochess::core::DeploymentService::moveToDeployment(
                deploymentPlayer,
                *trainingMap,
                *trainingFaction,
                1,
                autochess::core::GridPosition{1, 2});
        const auto occupiedBefore = deploymentPlayer;
        const auto occupiedResult =
            autochess::core::DeploymentService::moveToDeployment(
                deploymentPlayer,
                *trainingMap,
                *trainingFaction,
                2,
                autochess::core::GridPosition{1, 2});
        runner.check(
            legalDeploymentResult.success
                && !occupiedResult.success
                && occupiedResult.errorCode
                    == autochess::core::CommandErrorCode::TargetOccupied
                && !occupiedResult.message.empty()
                && playersAreEqual(deploymentPlayer, occupiedBefore),
            "Economy integration rejects an occupied deployment start atomically");

        auto repeatedDeathPlayer = firstRun.player;
        const auto firstDeathResult =
            autochess::core::RosterService::markDead(
                repeatedDeathPlayer, 4);
        const auto repeatedDeathBefore = repeatedDeathPlayer;
        const auto repeatedDeathResult =
            autochess::core::RosterService::markDead(
                repeatedDeathPlayer, 4);
        runner.check(
            firstDeathResult.success
                && !repeatedDeathResult.success
                && repeatedDeathResult.errorCode
                    == autochess::core::CommandErrorCode::UnitAlreadyDead
                && !repeatedDeathResult.message.empty()
                && playersAreEqual(
                    repeatedDeathPlayer, repeatedDeathBefore),
            "Economy integration rejects marking a dead unit twice atomically");

        auto activeRevivePlayer = firstRun.player;
        const auto activeReviveBefore = activeRevivePlayer;
        const auto activeReviveResult = autochess::core::RosterService::revive(
            activeRevivePlayer,
            4,
            bundle.gameConfig,
            bundle.units,
            *trainingFaction,
            bundle.factionModifiers);
        runner.check(
            !activeReviveResult.success
                && activeReviveResult.errorCode
                    == autochess::core::CommandErrorCode::UnitAlreadyActive
                && !activeReviveResult.message.empty()
                && playersAreEqual(activeRevivePlayer, activeReviveBefore),
            "Economy integration rejects reviving an active unit atomically");

        const EconomyScenarioRun secondRun = executeEconomyScenario(
            economyBundle, *trainingFaction, *trainingMap);
        runner.check(
            economyScenarioRunsAreEqual(firstRun, secondRun),
            "Economy integration reproduces the full scenario with one seed");

        std::cout
            << "[INFO] Economy integration:\n"
            << "initial=10\n"
            << "after_purchase=7,4,1\n"
            << "after_merge=2\n"
            << "after_sell=4\n"
            << "after_death=4\n"
            << "after_revive=2\n"
            << "after_refresh=0\n"
            << "final_active=" << firstRun.player.activeUnits.size() << '\n'
            << "final_dead=" << firstRun.player.deadUnits.size() << '\n'
            << "final_deployed=" << firstRun.player.deployments.size() << '\n';

        return runner.failureCount();
    }

    // 此函数检查配置错误是否包含预期类别、路径、行号和中文消息。
    bool hasExpectedConfigError(
        const autochess::core::ConfigError& error,
        const std::filesystem::path& path,
        const autochess::core::ConfigErrorCategory expectedCategory,
        const std::size_t expectedLine)
    {
        return error.category == expectedCategory
            && error.sourcePath == path
            && error.line == expectedLine
            && !error.message.empty()
            && !autochess::core::formatConfigError(error).empty();
    }

    // 此函数验证通用解析器按预期拒绝指定文件。
    bool parseMustFail(
        const std::filesystem::path& path,
        const autochess::core::ConfigErrorCategory expectedCategory,
        const std::size_t expectedLine)
    {
        autochess::core::ConfigDocument document;
        autochess::core::ConfigError error;
        const bool parsed = autochess::core::ConfigParser::parse(
            path, document, error);

        return !parsed
            && hasExpectedConfigError(
                error, path, expectedCategory, expectedLine);
    }

    // 验证游戏配置加载失败时返回预期错误并保持输出对象不变。
    bool loadGameMustFail(
        const std::filesystem::path& path,
        const autochess::core::ConfigErrorCategory expectedCategory,
        const std::size_t expectedLine)
    {
        autochess::core::GameConfig config;
        config.maxRounds = 99;
        config.randomSeed = 77;

        autochess::core::ConfigError error;
        const bool loaded = autochess::core::GameConfigLoader::load(
            path, config, error);

        return !loaded
            && config.maxRounds == 99
            && config.randomSeed == 77
            && hasExpectedConfigError(
                error, path, expectedCategory, expectedLine);
    }

    // 此函数验证技能加载失败时保留调用方原有集合。
    bool loadSkillsMustFail(
        const std::filesystem::path& path,
        const autochess::core::ConfigErrorCategory expectedCategory,
        const std::size_t expectedLine)
    {
        std::vector<autochess::core::SkillDefinition> skills(1);
        skills.front().id = "sentinel_skill";

        autochess::core::ConfigError error;
        const bool loaded = autochess::core::DefinitionConfigLoader::loadSkills(
            path, skills, error);

        return !loaded
            && skills.size() == 1
            && skills.front().id == "sentinel_skill"
            && hasExpectedConfigError(
                error, path, expectedCategory, expectedLine);
    }

    // 此函数验证单位加载失败时保留调用方原有集合。
    bool loadUnitsMustFail(
        const std::filesystem::path& path,
        const std::vector<autochess::core::SkillDefinition>& skills,
        const autochess::core::ConfigErrorCategory expectedCategory,
        const std::size_t expectedLine)
    {
        std::vector<autochess::core::UnitDefinition> units(1);
        units.front().id = "sentinel_unit";

        autochess::core::ConfigError error;
        const bool loaded = autochess::core::DefinitionConfigLoader::loadUnits(
            path, skills, units, error);

        return !loaded
            && units.size() == 1
            && units.front().id == "sentinel_unit"
            && hasExpectedConfigError(
                error, path, expectedCategory, expectedLine);
    }

    // 此函数验证分队加载失败时同时保留两个调用方输出集合。
    bool loadFactionsMustFail(
        const std::filesystem::path& path,
        const autochess::core::GameConfig& gameConfig,
        const std::vector<autochess::core::UnitDefinition>& units,
        const autochess::core::ConfigErrorCategory expectedCategory,
        const std::size_t expectedLine)
    {
        std::vector<autochess::core::FactionDefinition> factions(1);
        factions.front().id = "sentinel_faction";
        std::vector<autochess::core::FactionModifierDefinition> modifiers(1);
        modifiers.front().id = "sentinel_modifier";

        autochess::core::ConfigError error;
        const bool loaded = autochess::core::DefinitionConfigLoader::loadFactions(
            path, gameConfig, units, factions, modifiers, error);

        return !loaded
            && factions.size() == 1
            && factions.front().id == "sentinel_faction"
            && modifiers.size() == 1
            && modifiers.front().id == "sentinel_modifier"
            && hasExpectedConfigError(
                error, path, expectedCategory, expectedLine);
    }

    // 此函数验证地图加载失败时保留调用方已有地图对象。
    bool loadMapMustFailWithoutOverwrite(
        const std::filesystem::path& path,
        const autochess::core::ConfigErrorCategory expectedCategory,
        const std::size_t expectedLine)
    {
        autochess::core::MapDefinition map;
        map.id = "sentinel_map";
        map.width = 99;
        map.gridRows = {"sentinel_row"};

        autochess::core::ConfigError error;
        const bool loaded = autochess::core::MapConfigLoader::load(
            path, map, error);

        return !loaded
            && map.id == "sentinel_map"
            && map.width == 99
            && map.gridRows.size() == 1
            && map.gridRows.front() == "sentinel_row"
            && hasExpectedConfigError(
                error, path, expectedCategory, expectedLine);
    }

    // 此函数运行通用分段键值解析器的合法与非法输入测试。
    int runParserTests()
    {
        const std::filesystem::path dataDirectory =
            AUTOCHESS_TEST_DATA_DIR;
        TestRunner runner;

        autochess::core::ConfigDocument document;
        autochess::core::ConfigError error;
        const std::filesystem::path validPath =
            dataDirectory / "parser_valid.cfg";
        const bool validParsed = autochess::core::ConfigParser::parse(
            validPath, document, error);
        const bool validStructure = validParsed
            && document.sourcePath == validPath
            && document.sections.size() == 2
            && document.sections[0].type == "game"
            && document.sections[0].id.empty()
            && document.sections[0].fields.at("max_rounds").value == "3"
            && document.sections[0].fields.at("max_rounds").line == 4
            && document.sections[1].type == "unit"
            && document.sections[1].id == "training_guard";
        runner.check(validStructure,
            "ConfigParser reads valid sections, fields, IDs, and line numbers");

        runner.check(
            parseMustFail(
                dataDirectory / "parser_field_before_section.cfg",
                autochess::core::ConfigErrorCategory::Syntax,
                1),
            "ConfigParser rejects fields before a section");
        runner.check(
            parseMustFail(
                dataDirectory / "parser_missing_bracket.cfg",
                autochess::core::ConfigErrorCategory::Syntax,
                1),
            "ConfigParser rejects a section without closing bracket");
        runner.check(
            parseMustFail(
                dataDirectory / "parser_missing_equals.cfg",
                autochess::core::ConfigErrorCategory::Syntax,
                2),
            "ConfigParser rejects a field without equals sign");
        runner.check(
            parseMustFail(
                dataDirectory / "parser_duplicate_field.cfg",
                autochess::core::ConfigErrorCategory::DuplicateDefinition,
                3),
            "ConfigParser rejects duplicate fields");
        runner.check(
            parseMustFail(
                dataDirectory / "parser_duplicate_section.cfg",
                autochess::core::ConfigErrorCategory::DuplicateDefinition,
                3),
            "ConfigParser rejects duplicate sections");
        runner.check(
            parseMustFail(
                dataDirectory / "file_does_not_exist.cfg",
                autochess::core::ConfigErrorCategory::FileOpen,
                0),
            "ConfigParser reports a missing file");

        return runner.failureCount();
    }

    // 覆盖合法游戏配置以及必填、类型、范围和未知字段校验。
    int runGameConfigLoaderTests()
    {
        const std::filesystem::path dataDirectory = AUTOCHESS_DATA_DIR;
        const std::filesystem::path testDataDirectory =
            AUTOCHESS_TEST_DATA_DIR;
        TestRunner runner;

        // 加载正式配置并核对全部字段与冻结初始值一致。
        autochess::core::GameConfig config;
        autochess::core::ConfigError error;
        const std::filesystem::path validPath = dataDirectory / "game.cfg";
        const bool loaded = autochess::core::GameConfigLoader::load(
            validPath, config, error);
        const bool validValues = loaded
            && config.maxRounds == 3
            && config.preparationSeconds == 45
            && config.combatTimeoutSeconds == 60
            && config.startingGold == 10
            && config.roundIncome == 5
            && config.loserBonus == 2
            && config.shopSlots == 6
            && config.shopRefreshCost == 2
            && config.rosterCapacity == 8
            && config.sellRatio == 0.75
            && config.mergeRefundRatio == 0.40
            && config.reviveRatio == 0.50
            && config.maxUnitLevel == 3
            && config.randomSeed == 20260814;
        runner.check(
            validValues,
            "GameConfigLoader loads all fields from valid game.cfg");

        // 打印已通过校验的关键参数摘要供人工复核。
        if (loaded)
        {
            std::cout
                << "[INFO] GameConfig summary: rounds=" << config.maxRounds
                << ", preparation=" << config.preparationSeconds
                << "s, combat=" << config.combatTimeoutSeconds
                << "s, shop_slots=" << config.shopSlots
                << ", seed=" << config.randomSeed
                << '\n';
        }

        // 逐一验证缺字段、类型错误、范围错误和未知字段都会被准确拒绝。
        runner.check(
            loadGameMustFail(
                testDataDirectory / "game_missing_field.cfg",
                autochess::core::ConfigErrorCategory::MissingField,
                1),
            "GameConfigLoader rejects a missing required field");
        runner.check(
            loadGameMustFail(
                testDataDirectory / "game_invalid_integer.cfg",
                autochess::core::ConfigErrorCategory::TypeError,
                2),
            "GameConfigLoader rejects an invalid integer");
        runner.check(
            loadGameMustFail(
                testDataDirectory / "game_invalid_ratio.cfg",
                autochess::core::ConfigErrorCategory::RangeError,
                11),
            "GameConfigLoader rejects an out-of-range ratio");
        runner.check(
            loadGameMustFail(
                testDataDirectory / "game_unknown_field.cfg",
                autochess::core::ConfigErrorCategory::UnknownField,
                16),
            "GameConfigLoader rejects an unknown field");
        runner.check(
            loadGameMustFail(
                testDataDirectory / "game_seed_overflow.cfg",
                autochess::core::ConfigErrorCategory::RangeError,
                15),
            "GameConfigLoader rejects an overflowing random seed");

        return runner.failureCount();
    }

    // 此函数运行技能、单位和分队加载器的完整依赖链测试。
    int runDefinitionConfigLoaderTests()
    {
        const std::filesystem::path dataDirectory = AUTOCHESS_DATA_DIR;
        const std::filesystem::path testDataDirectory =
            AUTOCHESS_TEST_DATA_DIR;
        TestRunner runner;

        // 此代码段加载合法游戏配置供分队部署上限校验使用。
        autochess::core::GameConfig gameConfig;
        autochess::core::ConfigError error;
        const bool gameLoaded = autochess::core::GameConfigLoader::load(
            dataDirectory / "game.cfg", gameConfig, error);
        runner.check(
            gameLoaded,
            "Definition loaders receive a valid GameConfig dependency");

        // 此代码段按技能、单位、分队的固定顺序加载全部合法占位定义。
        std::vector<autochess::core::SkillDefinition> skills;
        std::vector<autochess::core::UnitDefinition> units;
        std::vector<autochess::core::FactionDefinition> factions;
        std::vector<autochess::core::FactionModifierDefinition> modifiers;
        const bool skillsLoaded =
            autochess::core::DefinitionConfigLoader::loadSkills(
                dataDirectory / "skills.cfg", skills, error);
        const bool unitsLoaded = skillsLoaded
            && autochess::core::DefinitionConfigLoader::loadUnits(
                dataDirectory / "units.cfg", skills, units, error);
        const bool factionsLoaded = unitsLoaded && gameLoaded
            && autochess::core::DefinitionConfigLoader::loadFactions(
                dataDirectory / "factions.cfg",
                gameConfig,
                units,
                factions,
                modifiers,
                error);

        // 此代码段核对合法定义的数量、关键 ID、枚举和值。
        const bool validDefinitions = factionsLoaded
            && skills.size() == 5
            && skills.front().id == "training_strike"
            && skills.front().effectType
                == autochess::core::SkillEffectType::Buff
            && skills.front().levelValues[2] == 16.0
            && units.size() == 5
            && units.front().id == "training_guard"
            && units.front().skillId == "training_strike"
            && units.front().tags.size() == 2
            && factions.size() == 3
            && factions.front().id == "training_team"
            && factions.front().maxDeployed == 4
            && modifiers.size() == 6
            && modifiers.front().factionId == "training_team"
            && modifiers.front().unitId == "training_guard"
            && modifiers.front().operation
                == autochess::core::FactionOperation::Multiply;
        runner.check(
            validDefinitions,
            "DefinitionConfigLoader loads valid skills, units, factions, and modifiers");

        // 此代码段打印已通过校验的定义数量供人工复核。
        if (factionsLoaded)
        {
            std::cout
                << "[INFO] Definition summary: skills=" << skills.size()
                << ", units=" << units.size()
                << ", factions=" << factions.size()
                << ", modifiers=" << modifiers.size()
                << '\n';
        }

        // 此代码段覆盖技能枚举、条件组合和三级列表错误。
        runner.check(
            loadSkillsMustFail(
                testDataDirectory / "skill_invalid_enum.cfg",
                autochess::core::ConfigErrorCategory::TypeError,
                4),
            "Skill loader rejects an unknown effect type");
        runner.check(
            loadSkillsMustFail(
                testDataDirectory / "skill_invalid_condition.cfg",
                autochess::core::ConfigErrorCategory::RangeError,
                10),
            "Skill loader rejects an invalid damage-skill duration");
        runner.check(
            loadSkillsMustFail(
                testDataDirectory / "skill_invalid_list.cfg",
                autochess::core::ConfigErrorCategory::TypeError,
                9),
            "Skill loader requires exactly three level values");

        // 此代码段覆盖单位必填字段、范围和技能引用错误。
        runner.check(
            loadUnitsMustFail(
                testDataDirectory / "unit_missing_field.cfg",
                skills,
                autochess::core::ConfigErrorCategory::MissingField,
                1),
            "Unit loader rejects a missing required field");
        runner.check(
            loadUnitsMustFail(
                testDataDirectory / "unit_invalid_range.cfg",
                skills,
                autochess::core::ConfigErrorCategory::RangeError,
                7),
            "Unit loader rejects magic resistance above 100");
        runner.check(
            loadUnitsMustFail(
                testDataDirectory / "unit_missing_skill_reference.cfg",
                skills,
                autochess::core::ConfigErrorCategory::ReferenceError,
                17),
            "Unit loader rejects a missing skill reference");
        runner.check(
            loadUnitsMustFail(
                testDataDirectory / "unit_invalid_heal_power.cfg",
                skills,
                autochess::core::ConfigErrorCategory::RangeError,
                6),
            "Unit loader rejects a healer without positive healing power");

        // 此代码段覆盖分队部署上限以及分队和单位引用错误。
        runner.check(
            loadFactionsMustFail(
                testDataDirectory / "faction_too_many_deployed.cfg",
                gameConfig,
                units,
                autochess::core::ConfigErrorCategory::RangeError,
                4),
            "Faction loader rejects deployment above roster capacity");
        runner.check(
            loadFactionsMustFail(
                testDataDirectory / "faction_missing_faction_reference.cfg",
                gameConfig,
                units,
                autochess::core::ConfigErrorCategory::ReferenceError,
                8),
            "Faction loader rejects a missing faction reference");
        runner.check(
            loadFactionsMustFail(
                testDataDirectory / "faction_missing_unit_reference.cfg",
                gameConfig,
                units,
                autochess::core::ConfigErrorCategory::ReferenceError,
                9),
            "Faction loader rejects a missing unit reference");

        return runner.failureCount();
    }

    // 此函数验证第 5 天正式内容表已完整映射到冻结配置字段。
    int runDay5ContentConfigTests()
    {
        TestRunner runner;
        autochess::core::ConfigBundle bundle;
        autochess::core::ConfigError error;

        // 此代码块加载正式配置并先验证五单位、五技能和三分队数量。
        const bool loaded = autochess::core::ConfigBundleLoader::load(
            AUTOCHESS_DATA_DIR,
            bundle,
            error);
        runner.check(
            loaded
                && bundle.units.size() == 5
                && bundle.skills.size() == 5
                && bundle.factions.size() == 3,
            "Day 5 content loads five units, five skills, and three factions");
        if (!loaded)
        {
            return runner.failureCount();
        }

        // 此代码块逐项核对五个技能的效果、目标和多目标数量。
        bool guardSkillValid = false;
        bool duelistSkillValid = false;
        bool rangerSkillValid = false;
        bool arcanistSkillValid = false;
        bool medicSkillValid = false;
        for (const autochess::core::SkillDefinition& skill : bundle.skills)
        {
            guardSkillValid = guardSkillValid
                || (skill.id == "training_strike"
                    && skill.effectType == autochess::core::SkillEffectType::Buff
                    && skill.targetRule == autochess::core::SkillTargetRule::Self
                    && skill.buffStat
                        == autochess::core::BuffStat::PhysicalDefense);
            duelistSkillValid = duelistSkillValid
                || (skill.id == "duelist_slash"
                    && skill.effectType == autochess::core::SkillEffectType::Damage
                    && skill.damageType == autochess::core::DamageType::Physical
                    && skill.targetCount == 1);
            rangerSkillValid = rangerSkillValid
                || (skill.id == "ranger_focus"
                    && skill.effectType == autochess::core::SkillEffectType::Buff
                    && skill.buffStat == autochess::core::BuffStat::AttackSpeed
                    && skill.modifierMode
                        == autochess::core::ModifierMode::Multiply);
            arcanistSkillValid = arcanistSkillValid
                || (skill.id == "arcane_burst"
                    && skill.effectType == autochess::core::SkillEffectType::Damage
                    && skill.damageType == autochess::core::DamageType::Magic
                    && skill.targetCount == 3);
            medicSkillValid = medicSkillValid
                || (skill.id == "field_mend"
                    && skill.effectType == autochess::core::SkillEffectType::Heal
                    && skill.targetRule
                        == autochess::core::SkillTargetRule::LowestHealthAlly
                    && skill.allowSelf);
        }
        runner.check(
            guardSkillValid
                && duelistSkillValid
                && rangerSkillValid
                && arcanistSkillValid
                && medicSkillValid,
            "Day 5 skills cover single damage, multi damage, healing, and buffs");

        // 此代码块核对单位角色、技能一对一引用和医疗普通行为。
        bool guardUnitValid = false;
        bool duelistUnitValid = false;
        bool rangerUnitValid = false;
        bool arcanistUnitValid = false;
        bool medicUnitValid = false;
        std::vector<std::string> referencedSkillIds;
        for (const autochess::core::UnitDefinition& unit : bundle.units)
        {
            referencedSkillIds.push_back(unit.skillId);
            guardUnitValid = guardUnitValid
                || (unit.id == "training_guard"
                    && unit.attackRange == 1.0
                    && unit.skillId == "training_strike");
            duelistUnitValid = duelistUnitValid
                || (unit.id == "duelist"
                    && unit.basicAction == autochess::core::BasicAction::Attack
                    && unit.skillId == "duelist_slash");
            rangerUnitValid = rangerUnitValid
                || (unit.id == "ranger"
                    && unit.attackRange > 3.0
                    && unit.skillId == "ranger_focus");
            arcanistUnitValid = arcanistUnitValid
                || (unit.id == "arcanist"
                    && unit.basicDamageType == autochess::core::DamageType::Magic
                    && unit.skillId == "arcane_burst");
            medicUnitValid = medicUnitValid
                || (unit.id == "medic"
                    && unit.basicAction == autochess::core::BasicAction::Heal
                    && unit.attackPower == 16
                    && unit.skillId == "field_mend");
        }

        // 此代码块拒绝两个正式单位共享同一个技能 ID。
        bool uniqueSkillReferences = true;
        for (std::size_t left = 0; left < referencedSkillIds.size(); ++left)
        {
            for (std::size_t right = left + 1;
                 right < referencedSkillIds.size();
                 ++right)
            {
                uniqueSkillReferences = uniqueSkillReferences
                    && referencedSkillIds[left] != referencedSkillIds[right];
            }
        }
        runner.check(
            guardUnitValid
                && duelistUnitValid
                && rangerUnitValid
                && arcanistUnitValid
                && medicUnitValid
                && uniqueSkillReferences,
            "Day 5 units cover the five required roles with independent skills");

        // 此代码块确认三种分队方向和显式单位修正均存在。
        bool defenseFactionValid = false;
        bool assaultFactionValid = false;
        bool routeFactionValid = false;
        for (const autochess::core::FactionDefinition& faction : bundle.factions)
        {
            defenseFactionValid = defenseFactionValid
                || (faction.id == "training_team"
                    && faction.initialGuard == 100
                    && faction.maxDeployed == 4);
            assaultFactionValid = assaultFactionValid
                || (faction.id == "assault_team"
                    && faction.maxDeployed == 5
                    && faction.priceMultiplier == 1.0);
            routeFactionValid = routeFactionValid
                || (faction.id == "route_team"
                    && faction.maxDeployed == 5
                    && faction.priceMultiplier == 0.9);
        }
        bool modifiersUseExplicitUnitIds = true;
        for (const autochess::core::FactionModifierDefinition& modifier :
             bundle.factionModifiers)
        {
            modifiersUseExplicitUnitIds = modifiersUseExplicitUnitIds
                && modifier.unitId != "*";
        }
        runner.check(
            defenseFactionValid
                && assaultFactionValid
                && routeFactionValid
                && modifiersUseExplicitUnitIds,
            "Day 5 factions represent defense, assault, and route economy roles");

        return runner.failureCount();
    }

    // 此函数验证抽象接口、五个具体单位角色和配置技能等级读取。
    int runUnitSkillInterfaceTests()
    {
        static_assert(std::is_abstract_v<autochess::core::Unit>);
        static_assert(std::is_abstract_v<autochess::core::ISkillUser>);
        static_assert(std::is_abstract_v<autochess::core::ActiveSkillUnit>);
        static_assert(std::is_abstract_v<autochess::core::Skill>);

        TestRunner runner;
        autochess::core::ConfigBundle bundle;
        autochess::core::ConfigError error;

        // 此代码块加载正式定义并按 ID 找到五个单位和奥术技能。
        const bool loaded = autochess::core::ConfigBundleLoader::load(
            AUTOCHESS_DATA_DIR,
            bundle,
            error);
        const autochess::core::UnitDefinition* guardDefinition = nullptr;
        const autochess::core::UnitDefinition* duelistDefinition = nullptr;
        const autochess::core::UnitDefinition* rangerDefinition = nullptr;
        const autochess::core::UnitDefinition* arcanistDefinition = nullptr;
        const autochess::core::UnitDefinition* medicDefinition = nullptr;
        for (const autochess::core::UnitDefinition& definition : bundle.units)
        {
            if (definition.id == "training_guard")
            {
                guardDefinition = &definition;
            }
            else if (definition.id == "duelist")
            {
                duelistDefinition = &definition;
            }
            else if (definition.id == "ranger")
            {
                rangerDefinition = &definition;
            }
            else if (definition.id == "arcanist")
            {
                arcanistDefinition = &definition;
            }
            else if (definition.id == "medic")
            {
                medicDefinition = &definition;
            }
        }

        const autochess::core::SkillDefinition* arcaneSkillDefinition = nullptr;
        for (const autochess::core::SkillDefinition& definition : bundle.skills)
        {
            if (definition.id == "arcane_burst")
            {
                arcaneSkillDefinition = &definition;
                break;
            }
        }
        const bool definitionsFound = loaded
            && guardDefinition != nullptr
            && duelistDefinition != nullptr
            && rangerDefinition != nullptr
            && arcanistDefinition != nullptr
            && medicDefinition != nullptr
            && arcaneSkillDefinition != nullptr;
        runner.check(
            definitionsFound,
            "Unit and skill interfaces receive all formal definitions");
        if (!definitionsFound)
        {
            return runner.failureCount();
        }

        // 此代码块实例化五个具体单位并核对角色和配置驱动技能 ID。
        const autochess::core::IronGuardUnit guard(*guardDefinition);
        const autochess::core::DuelistUnit duelist(*duelistDefinition);
        const autochess::core::RangerUnit ranger(*rangerDefinition);
        const autochess::core::ArcanistUnit arcanist(*arcanistDefinition);
        const autochess::core::MedicUnit medic(*medicDefinition);
        runner.check(
            guard.role() == autochess::core::UnitRole::Defender
                && duelist.role() == autochess::core::UnitRole::MeleeDamage
                && ranger.role()
                    == autochess::core::UnitRole::RangedPhysical
                && arcanist.role()
                    == autochess::core::UnitRole::MultiTargetMagic
                && medic.role() == autochess::core::UnitRole::Healer
                && guard.skillId() == "training_strike"
                && medic.skillId() == "field_mend",
            "Five concrete units expose distinct roles and configured skills");

        // 此代码块验证主动技能前置条件和安全的三级数值查询。
        const autochess::core::ConfiguredSkill arcaneSkill(
            *arcaneSkillDefinition);
        const auto levelThreeValue = arcaneSkill.valueForLevel(3);
        runner.check(
            guard.canReleaseSkill(10, 10, false)
                && !guard.canReleaseSkill(9, 10, false)
                && !guard.canReleaseSkill(10, 10, true)
                && levelThreeValue.has_value()
                && levelThreeValue.value() == 42.0
                && !arcaneSkill.valueForLevel(0).has_value()
                && !arcaneSkill.valueForLevel(4).has_value(),
            "Active skill interfaces validate mana and level boundaries");

        return runner.failureCount();
    }

    // 此函数验证技力恢复限制和三种技能目标规则的确定性结果。
    int runSkillSystemRuleTests()
    {
        TestRunner runner;

        // 此辅助代码块用固定误差比较技力测试中的小数结果。
        const auto manaNearlyEqual = [](const double left, const double right)
        {
            return std::abs(left - right) <= 1.0e-9;
        };

        // 此辅助代码块创建带有效战斗身份、位置和生命值的技能测试单位。
        const auto makeUnit = [](
            const autochess::core::BattleUnitId id,
            const autochess::core::MapSide side,
            const autochess::core::BattlePosition position,
            const double health)
        {
            autochess::core::BattleUnit unit;
            unit.id = id;
            unit.side = side;
            unit.position = position;
            unit.stats.maxHealth = 100.0;
            unit.health = health;
            unit.maxMana = 10.0;
            return unit;
        };

        // 此代码块验证每秒一点恢复、最大值封顶和满技力释放条件。
        auto manaUnit = makeUnit(
            1, autochess::core::MapSide::A, {0.0, 0.0}, 100.0);
        manaUnit.currentMana = 8.5;
        autochess::core::SkillSystem::regenerateMana(manaUnit, 0.5);
        autochess::core::SkillSystem::regenerateMana(manaUnit, 5.0);
        runner.check(
            manaNearlyEqual(manaUnit.currentMana, 10.0)
                && autochess::core::SkillSystem::canRelease(manaUnit),
            "Skill mana regenerates at one per second and stops at maximum");

        // 此代码块验证技能生效中和单位死亡后均不恢复技力也不能释放。
        manaUnit.currentMana = 4.0;
        manaUnit.activeSkill.active = true;
        autochess::core::SkillSystem::regenerateMana(manaUnit, 2.0);
        const bool activeBlocked = manaNearlyEqual(manaUnit.currentMana, 4.0)
            && !autochess::core::SkillSystem::canRelease(manaUnit);
        manaUnit.activeSkill.active = false;
        manaUnit.state = autochess::core::BattleUnitState::Dead;
        autochess::core::SkillSystem::regenerateMana(manaUnit, 2.0);
        runner.check(
            activeBlocked
                && manaNearlyEqual(manaUnit.currentMana, 4.0)
                && !autochess::core::SkillSystem::canRelease(manaUnit),
            "Skill mana pauses while active and after unit death");

        // 此代码块验证敌方技能按距离和编号选出范围内前两个目标。
        const auto caster = makeUnit(
            10, autochess::core::MapSide::A, {0.0, 0.0}, 100.0);
        auto enemyHigherId = makeUnit(
            3, autochess::core::MapSide::B, {1.0, 0.0}, 100.0);
        auto enemyLowerId = enemyHigherId;
        enemyLowerId.id = 2;
        auto fartherEnemy = makeUnit(
            1, autochess::core::MapSide::B, {2.0, 0.0}, 100.0);
        auto outsideEnemy = makeUnit(
            4, autochess::core::MapSide::B, {4.0, 0.0}, 100.0);
        auto friendlyUnit = makeUnit(
            5, autochess::core::MapSide::A, {0.5, 0.0}, 100.0);
        autochess::core::SkillDefinition enemySkill;
        enemySkill.targetRule = autochess::core::SkillTargetRule::Enemy;
        enemySkill.targetCount = 2;
        enemySkill.effectRange = 3.0;
        const std::vector<autochess::core::BattleUnit> enemyUnits = {
            outsideEnemy,
            enemyHigherId,
            fartherEnemy,
            friendlyUnit,
            enemyLowerId,
            caster};
        const auto enemyTargets =
            autochess::core::SkillSystem::selectTargets(
                caster, enemySkill, enemyUnits);
        runner.check(
            enemyTargets
                == std::vector<autochess::core::BattleUnitId>{2, 3},
            "Skill enemy targets use distance then ID with count and range");

        // 此代码块验证治疗技能忽略满血和禁用的自身并按生命比例、编号选目标。
        auto woundedLowerId = makeUnit(
            6, autochess::core::MapSide::A, {1.0, 0.0}, 25.0);
        auto woundedHigherId = woundedLowerId;
        woundedHigherId.id = 7;
        auto fullAlly = makeUnit(
            8, autochess::core::MapSide::A, {1.0, 0.0}, 100.0);
        auto woundedCaster = caster;
        woundedCaster.health = 10.0;
        autochess::core::SkillDefinition healingSkill;
        healingSkill.targetRule =
            autochess::core::SkillTargetRule::LowestHealthAlly;
        healingSkill.targetCount = 1;
        healingSkill.effectRange = 3.0;
        healingSkill.allowSelf = false;
        const std::vector<autochess::core::BattleUnit> healingUnits = {
            woundedHigherId,
            fullAlly,
            woundedCaster,
            woundedLowerId};
        const auto allyTargets =
            autochess::core::SkillSystem::selectTargets(
                woundedCaster, healingSkill, healingUnits);
        healingSkill.allowSelf = true;
        const auto selfTarget =
            autochess::core::SkillSystem::selectTargets(
                woundedCaster, healingSkill, healingUnits);
        runner.check(
            allyTargets
                    == std::vector<autochess::core::BattleUnitId>{6}
                && selfTarget
                    == std::vector<autochess::core::BattleUnitId>{10},
            "Skill healing targets use health ratio, ID, and self policy");

        // 此代码块验证自身目标规则只返回存活施法者自己的编号。
        autochess::core::SkillDefinition selfSkill;
        selfSkill.targetRule = autochess::core::SkillTargetRule::Self;
        selfSkill.targetCount = 1;
        selfSkill.effectRange = 0.0;
        selfSkill.allowSelf = true;
        const auto selfTargets =
            autochess::core::SkillSystem::selectTargets(
                caster, selfSkill, enemyUnits);
        runner.check(
            selfTargets
                == std::vector<autochess::core::BattleUnitId>{10},
            "Skill self target rule selects only its living caster");

        return runner.failureCount();
    }

    // 此函数验证物理、法术、治疗和限时增益技能的实际数值效果。
    int runSkillEffectTests()
    {
        TestRunner runner;

        // 此辅助代码块创建具有完整基础属性、实时属性和满技力的施法测试单位。
        const auto makeUnit = [](
            const autochess::core::BattleUnitId id,
            const autochess::core::MapSide side,
            const autochess::core::BattlePosition position)
        {
            autochess::core::BattleUnit unit;
            unit.id = id;
            unit.identity = {"skill_unit", 1};
            unit.side = side;
            unit.position = position;
            unit.baseStats.maxHealth = 100.0;
            unit.baseStats.attackPower = 20.0;
            unit.baseStats.physicalDefense = 7.0;
            unit.baseStats.magicResistance = 20.0;
            unit.baseStats.moveSpeed = 1.0;
            unit.baseStats.attackSpeed = 100.0;
            unit.stats = unit.baseStats;
            unit.health = unit.stats.maxHealth;
            unit.currentMana = 10.0;
            unit.maxMana = 10.0;
            return unit;
        };

        // 此代码块验证单体物理技能扣除防御、消耗全部技力并能造成死亡。
        auto physicalCaster = makeUnit(
            1, autochess::core::MapSide::A, {0.0, 0.0});
        physicalCaster.skillId = "physical_skill";
        auto physicalTarget = makeUnit(
            2, autochess::core::MapSide::B, {1.0, 0.0});
        physicalTarget.health = 15.0;
        autochess::core::SkillDefinition physicalSkill;
        physicalSkill.id = "physical_skill";
        physicalSkill.effectType = autochess::core::SkillEffectType::Damage;
        physicalSkill.targetRule = autochess::core::SkillTargetRule::Enemy;
        physicalSkill.targetCount = 1;
        physicalSkill.effectRange = 2.0;
        physicalSkill.damageType = autochess::core::DamageType::Physical;
        physicalSkill.levelValues = {22.0, 34.0, 51.0};
        std::vector<autochess::core::BattleUnit> physicalUnits = {
            physicalCaster,
            physicalTarget};
        const bool physicalReleased = autochess::core::SkillSystem::release(
            1, physicalSkill, physicalUnits);
        runner.check(
            physicalReleased
                && physicalUnits[0].currentMana == 0.0
                && physicalUnits[1].health == 0.0
                && physicalUnits[1].state
                    == autochess::core::BattleUnitState::Dead,
            "Physical skill uses defense, consumes mana, and marks death");

        // 此代码块验证多体法术技能按各目标法抗独立计算伤害。
        auto magicCaster = makeUnit(
            3, autochess::core::MapSide::A, {0.0, 0.0});
        magicCaster.skillId = "magic_skill";
        auto magicTargetA = makeUnit(
            4, autochess::core::MapSide::B, {1.0, 0.0});
        auto magicTargetB = makeUnit(
            5, autochess::core::MapSide::B, {2.0, 0.0});
        magicTargetB.stats.magicResistance = 50.0;
        autochess::core::SkillDefinition magicSkill;
        magicSkill.id = "magic_skill";
        magicSkill.effectType = autochess::core::SkillEffectType::Damage;
        magicSkill.targetRule = autochess::core::SkillTargetRule::Enemy;
        magicSkill.targetCount = 2;
        magicSkill.effectRange = 3.0;
        magicSkill.damageType = autochess::core::DamageType::Magic;
        magicSkill.levelValues = {20.0, 30.0, 45.0};
        std::vector<autochess::core::BattleUnit> magicUnits = {
            magicTargetB,
            magicCaster,
            magicTargetA};
        const bool magicReleased = autochess::core::SkillSystem::release(
            3, magicSkill, magicUnits);
        runner.check(
            magicReleased
                && std::abs(magicUnits[0].health - 90.0) <= 1.0e-9
                && std::abs(magicUnits[2].health - 84.0) <= 1.0e-9,
            "Magic skill damages multiple targets using each resistance");

        // 此代码块验证单体治疗技能不会使受伤友军超过最大生命值。
        auto healingCaster = makeUnit(
            6, autochess::core::MapSide::A, {0.0, 0.0});
        healingCaster.skillId = "healing_skill";
        auto healingTarget = makeUnit(
            7, autochess::core::MapSide::A, {1.0, 0.0});
        healingTarget.health = 90.0;
        autochess::core::SkillDefinition healingSkill;
        healingSkill.id = "healing_skill";
        healingSkill.effectType = autochess::core::SkillEffectType::Heal;
        healingSkill.targetRule =
            autochess::core::SkillTargetRule::LowestHealthAlly;
        healingSkill.targetCount = 1;
        healingSkill.effectRange = 2.0;
        healingSkill.damageType = autochess::core::DamageType::None;
        healingSkill.levelValues = {24.0, 36.0, 54.0};
        healingSkill.allowSelf = true;
        std::vector<autochess::core::BattleUnit> healingUnits = {
            healingCaster,
            healingTarget};
        const bool healingReleased = autochess::core::SkillSystem::release(
            6, healingSkill, healingUnits);
        runner.check(
            healingReleased
                && healingUnits[0].currentMana == 0.0
                && healingUnits[1].health == 100.0,
            "Healing skill restores health without exceeding maximum");

        // 此代码块验证加法防御增益持续三秒并在第一百八十帧后恢复基础值。
        auto defenseCaster = makeUnit(
            8, autochess::core::MapSide::A, {0.0, 0.0});
        defenseCaster.skillId = "defense_buff";
        autochess::core::SkillDefinition defenseBuff;
        defenseBuff.id = "defense_buff";
        defenseBuff.effectType = autochess::core::SkillEffectType::Buff;
        defenseBuff.targetRule = autochess::core::SkillTargetRule::Self;
        defenseBuff.targetCount = 1;
        defenseBuff.effectRange = 0.0;
        defenseBuff.levelValues = {8.0, 12.0, 16.0};
        defenseBuff.durationSeconds = 3.0;
        defenseBuff.buffStat = autochess::core::BuffStat::PhysicalDefense;
        defenseBuff.modifierMode = autochess::core::ModifierMode::Add;
        defenseBuff.allowSelf = true;
        defenseBuff.allowMove = false;
        defenseBuff.allowBasicAction = true;
        std::vector<autochess::core::BattleUnit> defenseUnits = {
            defenseCaster};
        const bool defenseReleased = autochess::core::SkillSystem::release(
            8, defenseBuff, defenseUnits);
        const bool defenseStarted = defenseReleased
            && defenseUnits[0].activeSkill.active
            && defenseUnits[0].activeSkill.remainingFrames == 180
            && defenseUnits[0].stats.physicalDefense == 15.0
            && !defenseUnits[0].activeSkill.allowMove
            && defenseUnits[0].activeSkill.allowBasicAction;
        for (int frame = 0; frame < 179; ++frame)
        {
            autochess::core::SkillSystem::advanceActiveSkill(
                defenseUnits[0]);
        }
        const bool defenseLastFrame = defenseUnits[0].activeSkill.active
            && defenseUnits[0].activeSkill.remainingFrames == 1
            && defenseUnits[0].stats.physicalDefense == 15.0;
        autochess::core::SkillSystem::advanceActiveSkill(defenseUnits[0]);
        runner.check(
            defenseStarted
                && defenseLastFrame
                && !defenseUnits[0].activeSkill.active
                && defenseUnits[0].stats.physicalDefense == 7.0,
            "Additive buff lasts exact frames and restores base defense");

        // 此代码块验证乘法攻击速度增益从基础值计算且不会累积旧结果。
        auto speedCaster = makeUnit(
            9, autochess::core::MapSide::A, {0.0, 0.0});
        speedCaster.skillId = "speed_buff";
        autochess::core::SkillDefinition speedBuff = defenseBuff;
        speedBuff.id = "speed_buff";
        speedBuff.levelValues = {1.25, 1.5, 2.0};
        speedBuff.durationSeconds = 4.0;
        speedBuff.buffStat = autochess::core::BuffStat::AttackSpeed;
        speedBuff.modifierMode = autochess::core::ModifierMode::Multiply;
        std::vector<autochess::core::BattleUnit> speedUnits = {speedCaster};
        const bool speedReleased = autochess::core::SkillSystem::release(
            9, speedBuff, speedUnits);
        runner.check(
            speedReleased
                && speedUnits[0].activeSkill.remainingFrames == 240
                && speedUnits[0].stats.attackSpeed == 125.0,
            "Multiplicative buff derives attack speed from base stats");

        // 此代码块验证没有合法目标时技能失败且不消耗技力或修改生命值。
        auto failedCaster = makeUnit(
            10, autochess::core::MapSide::A, {0.0, 0.0});
        failedCaster.skillId = physicalSkill.id;
        std::vector<autochess::core::BattleUnit> failedUnits = {
            failedCaster};
        const bool failedRelease = autochess::core::SkillSystem::release(
            10, physicalSkill, failedUnits);
        runner.check(
            !failedRelease
                && failedUnits[0].currentMana == 10.0
                && failedUnits[0].health == 100.0,
            "Skill release failure leaves mana and health unchanged");

        return runner.failureCount();
    }

    // 此函数运行第一张正式地图和全部路线非法输入测试。
    int runMapConfigLoaderTests()
    {
        const std::filesystem::path dataDirectory = AUTOCHESS_DATA_DIR;
        const std::filesystem::path testDataDirectory =
            AUTOCHESS_TEST_DATA_DIR;
        TestRunner runner;

        // 此代码段加载第一张正式地图供合法结构和路线数量测试使用。
        autochess::core::MapDefinition map;
        autochess::core::ConfigError error;
        const std::filesystem::path validPath =
            dataDirectory / "maps" / "map_01.map";
        const bool loaded = autochess::core::MapConfigLoader::load(
            validPath, map, error);

        // 此代码段统计双方路线并确认对称地图的每条路线长度一致。
        int routeACount = 0;
        int routeBCount = 0;
        bool everyRouteHasTwelvePoints = loaded;
        for (const autochess::core::Route& route : map.routes)
        {
            routeACount += route.side == autochess::core::MapSide::A ? 1 : 0;
            routeBCount += route.side == autochess::core::MapSide::B ? 1 : 0;
            everyRouteHasTwelvePoints =
                everyRouteHasTwelvePoints && route.points.size() == 12;
        }

        // 此代码段核对合法地图加载后的全部关键字段和路线摘要。
        const bool validValues = loaded
            && map.id == "map_01"
            && map.name == "对称双路训练场"
            && map.width == 11
            && map.height == 7
            && map.gridRows.size() == 7
            && map.gridRows.front() == "###########"
            && map.gridRows[2] == "#A..###..B#"
            && map.routes.size() == 4
            && routeACount == 2
            && routeBCount == 2
            && everyRouteHasTwelvePoints;
        runner.check(
            validValues,
            "MapConfigLoader loads map_01 grid and four routes");

        // 此代码段在合法地图加载成功后打印便于人工复核的摘要。
        if (loaded)
        {
            std::cout
                << "[INFO] Map summary: id=" << map.id
                << ", size=" << map.width << 'x' << map.height
                << ", routes=" << map.routes.size()
                << '\n';
        }

        // 此代码段加载第二张正式地图并在失败时输出带路径和行号的诊断信息。
        autochess::core::MapDefinition variedMap;
        autochess::core::ConfigError variedMapError;
        const std::filesystem::path variedMapPath =
            dataDirectory / "maps" / "map_02.map";
        const bool variedMapLoaded = autochess::core::MapConfigLoader::load(
            variedMapPath, variedMap, variedMapError);
        if (!variedMapLoaded)
        {
            std::cerr
                << autochess::core::formatConfigError(variedMapError)
                << '\n';
        }

        // 此代码段统计第二张地图双方路线数量以及最短和最长路线长度。
        int variedRouteACount = 0;
        int variedRouteBCount = 0;
        std::size_t shortestRouteLength = 0;
        std::size_t longestRouteLength = 0;
        if (variedMapLoaded && !variedMap.routes.empty())
        {
            shortestRouteLength = variedMap.routes.front().points.size();
            longestRouteLength = shortestRouteLength;
            for (const autochess::core::Route& route : variedMap.routes)
            {
                variedRouteACount +=
                    route.side == autochess::core::MapSide::A ? 1 : 0;
                variedRouteBCount +=
                    route.side == autochess::core::MapSide::B ? 1 : 0;
                if (route.points.size() < shortestRouteLength)
                {
                    shortestRouteLength = route.points.size();
                }
                if (route.points.size() > longestRouteLength)
                {
                    longestRouteLength = route.points.size();
                }
            }
        }

        // 此代码段核对第二张地图尺寸、部署路线数量和三档路线长度差异。
        const bool variedMapValid = variedMapLoaded
            && variedMap.id == "map_02"
            && variedMap.name == "多路线峡谷"
            && variedMap.width == 15
            && variedMap.height == 9
            && variedMap.gridRows.size() == 9
            && variedMap.routes.size() == 6
            && variedRouteACount == 3
            && variedRouteBCount == 3
            && shortestRouteLength == 12
            && longestRouteLength == 21;
        runner.check(
            variedMapValid,
            "MapConfigLoader loads map_02 with varied route lengths");

        // 此代码段打印第二张地图的路线长度摘要供人工复核。
        if (variedMapLoaded)
        {
            std::cout
                << "[INFO] Map summary: id=" << variedMap.id
                << ", size=" << variedMap.width << 'x' << variedMap.height
                << ", routes=" << variedMap.routes.size()
                << ", shortest=" << shortestRouteLength
                << ", longest=" << longestRouteLength
                << '\n';
        }

        // 此代码段确认文件打开失败时不会覆盖调用方原有地图对象。
        runner.check(
            loadMapMustFailWithoutOverwrite(
                dataDirectory / "maps" / "missing.map",
                autochess::core::ConfigErrorCategory::FileOpen,
                0),
            "MapConfigLoader reports a missing file without overwriting output");

        // 此代码段覆盖路线必填字段、首点和阵营起点的校验。
        runner.check(
            loadMapMustFailWithoutOverwrite(
                testDataDirectory / "map_missing_points.map",
                autochess::core::ConfigErrorCategory::MissingField,
                13),
            "MapConfigLoader rejects a route without points");
        runner.check(
            loadMapMustFailWithoutOverwrite(
                testDataDirectory / "map_first_point_mismatch.map",
                autochess::core::ConfigErrorCategory::MapValidation,
                16),
            "MapConfigLoader rejects points whose first item differs from start");
        runner.check(
            loadMapMustFailWithoutOverwrite(
                testDataDirectory / "map_wrong_side_start.map",
                autochess::core::ConfigErrorCategory::MapValidation,
                15),
            "MapConfigLoader rejects a route starting on the opposing deployment");

        // 此代码段覆盖部署格缺少路线或对应多条路线的全局校验。
        runner.check(
            loadMapMustFailWithoutOverwrite(
                testDataDirectory / "map_missing_deployment_route.map",
                autochess::core::ConfigErrorCategory::MapValidation,
                10),
            "MapConfigLoader requires one route for every deployment cell");
        runner.check(
            loadMapMustFailWithoutOverwrite(
                testDataDirectory / "map_duplicate_deployment_route.map",
                autochess::core::ConfigErrorCategory::MapValidation,
                10),
            "MapConfigLoader rejects multiple routes for one deployment cell");

        // 此代码段覆盖路线越界、障碍、斜向连接和错误终点校验。
        runner.check(
            loadMapMustFailWithoutOverwrite(
                testDataDirectory / "map_route_out_of_bounds.map",
                autochess::core::ConfigErrorCategory::MapValidation,
                16),
            "MapConfigLoader rejects an out-of-bounds route point");
        runner.check(
            loadMapMustFailWithoutOverwrite(
                testDataDirectory / "map_route_through_obstacle.map",
                autochess::core::ConfigErrorCategory::MapValidation,
                16),
            "MapConfigLoader rejects a route through an obstacle");
        runner.check(
            loadMapMustFailWithoutOverwrite(
                testDataDirectory / "map_route_diagonal.map",
                autochess::core::ConfigErrorCategory::MapValidation,
                16),
            "MapConfigLoader rejects a diagonal route segment");
        runner.check(
            loadMapMustFailWithoutOverwrite(
                testDataDirectory / "map_route_wrong_guard.map",
                autochess::core::ConfigErrorCategory::MapValidation,
                16),
            "MapConfigLoader requires routes to end at the enemy guard");

        return runner.failureCount();
    }

    // 此函数验证统一入口能原子地加载全部正式配置并输出可复核摘要。
    int runConfigBundleLoaderTests()
    {
        // 此代码段准备正式数据目录、非法目录和测试结果累计器。
        const std::filesystem::path dataDirectory = AUTOCHESS_DATA_DIR;
        const std::filesystem::path testDataDirectory =
            AUTOCHESS_TEST_DATA_DIR;
        TestRunner runner;

        // 此代码段通过统一入口加载完整配置并在失败时输出详细诊断。
        autochess::core::ConfigBundle bundle;
        autochess::core::ConfigError error;
        const bool loaded = autochess::core::ConfigBundleLoader::load(
            dataDirectory, bundle, error);
        if (!loaded)
        {
            std::cerr << autochess::core::formatConfigError(error) << '\n';
        }

        // 此代码段核对整包中的全局参数、定义数量和固定地图顺序。
        const bool validBundle = loaded
            && bundle.gameConfig.maxRounds == 3
            && bundle.skills.size() == 5
            && bundle.units.size() == 5
            && bundle.factions.size() == 3
            && bundle.factionModifiers.size() == 6
            && bundle.maps.size() == 2
            && bundle.maps[0].id == "map_01"
            && bundle.maps[1].id == "map_02";
        runner.check(
            validBundle,
            "ConfigBundleLoader loads all definitions and two maps in order");

        // 此代码段打印整包定义数量供人工确认全部依赖均已加载。
        if (loaded)
        {
            std::cout
                << "[INFO] Config bundle summary: skills="
                << bundle.skills.size()
                << ", units=" << bundle.units.size()
                << ", factions=" << bundle.factions.size()
                << ", modifiers=" << bundle.factionModifiers.size()
                << ", maps=" << bundle.maps.size()
                << '\n';
        }

        // 此代码段为每张已加载地图计算并打印最短和最长路线长度。
        for (const autochess::core::MapDefinition& loadedMap : bundle.maps)
        {
            std::size_t shortestLength = 0;
            std::size_t longestLength = 0;
            if (!loadedMap.routes.empty())
            {
                shortestLength = loadedMap.routes.front().points.size();
                longestLength = shortestLength;
                for (const autochess::core::Route& route : loadedMap.routes)
                {
                    if (route.points.size() < shortestLength)
                    {
                        shortestLength = route.points.size();
                    }
                    if (route.points.size() > longestLength)
                    {
                        longestLength = route.points.size();
                    }
                }
            }

            // 此代码段输出当前地图的路线数量和长度范围供人工复核。
            std::cout
                << "[INFO] Map " << loadedMap.id
                << ": routes=" << loadedMap.routes.size()
                << ", shortest=" << shortestLength
                << ", longest=" << longestLength
                << '\n';
        }

        // 此代码段在调用失败前放入哨兵数据以验证整包加载的原子性。
        autochess::core::ConfigBundle unchangedBundle;
        unchangedBundle.gameConfig.maxRounds = 99;
        autochess::core::MapDefinition sentinelMap;
        sentinelMap.id = "sentinel_map";
        unchangedBundle.maps.push_back(sentinelMap);

        // 此代码段从不存在的目录加载并核对错误信息与原有哨兵数据。
        autochess::core::ConfigError missingError;
        const std::filesystem::path missingDirectory =
            testDataDirectory / "missing_bundle_data";
        const bool missingLoaded = autochess::core::ConfigBundleLoader::load(
            missingDirectory, unchangedBundle, missingError);
        const bool failureIsAtomic = !missingLoaded
            && unchangedBundle.gameConfig.maxRounds == 99
            && unchangedBundle.maps.size() == 1
            && unchangedBundle.maps.front().id == "sentinel_map"
            && hasExpectedConfigError(
                missingError,
                missingDirectory / "game.cfg",
                autochess::core::ConfigErrorCategory::FileOpen,
                0);
        runner.check(
            failureIsAtomic,
            "ConfigBundleLoader preserves existing output after a load failure");

        return runner.failureCount();
    }

    int runBattleSetupServiceTests()
    {
        TestRunner runner;

        autochess::core::ConfigBundle bundle;
        autochess::core::ConfigError loadError;
        const bool loaded = autochess::core::ConfigBundleLoader::load(
            AUTOCHESS_DATA_DIR,
            bundle,
            loadError);
        runner.check(loaded, "Battle setup loads the formal configuration");
        if (!loaded || bundle.factions.empty() || bundle.maps.empty())
        {
            return runner.failureCount();
        }

        auto playerA = autochess::core::PlayerStateService::createInitial(
            autochess::core::MapSide::A,
            bundle.gameConfig,
            bundle.factions.front());
        auto playerB = autochess::core::PlayerStateService::createInitial(
            autochess::core::MapSide::B,
            bundle.gameConfig,
            bundle.factions.front());

        playerA.activeUnits = {
            {10, {bundle.units.front().id, 1}, autochess::core::MapSide::A},
            {11, {bundle.units.front().id, 1}, autochess::core::MapSide::A}};
        playerA.deployments.emplace(
            autochess::core::GridPosition{1, 2},
            10);
        playerA.reserveSlots[0] = 11;

        playerB.activeUnits = {
            {20, {bundle.units.front().id, 2}, autochess::core::MapSide::B}};
        playerB.deployments.emplace(
            autochess::core::GridPosition{9, 2},
            20);

        std::vector<autochess::core::BattleUnit> units;
        std::string setupError;
        const bool created = autochess::core::BattleSetupService::createUnits(
            playerA,
            playerB,
            bundle.maps.front(),
            bundle.units,
            bundle.factions,
            bundle.factionModifiers,
            units,
            setupError);
        runner.check(
            created && setupError.empty() && units.size() == 2,
            "Battle setup creates only deployed units");

        const bool firstUnitIsCorrect = created
            && units[0].id == 1
            && units[0].ownedUnitId == 10
            && units[0].side == autochess::core::MapSide::A
            && units[0].position
                == autochess::core::BattlePosition{1.5, 2.5}
            && units[0].nextRoutePointIndex == 1
            && units[0].routePoints.size() == 12
            && units[0].health == 160.0
            && units[0].stats.attackPower == 22.0
            && units[0].stats.physicalDefense == 22.0
            && units[0].baseStats.attackPower == units[0].stats.attackPower
            && units[0].skillId == "training_strike"
            && units[0].currentMana == 0.0
            && units[0].maxMana == 10.0
            && !units[0].activeSkill.active;
        runner.check(
            firstUnitIsCorrect,
            "Battle setup preserves identity, route, and grid-center position");

        const bool secondUnitIsCorrect = created
            && units[1].id == 2
            && units[1].ownedUnitId == 20
            && units[1].identity.level == 2
            && units[1].stats.maxHealth == 240.0
            && units[1].stats.attackPower == 33.0
            && units[1].stats.physicalDefense == 31.0
            && units[1].stats.guardDamage == 8
            && units[1].health == units[1].stats.maxHealth;
        runner.check(
            secondUnitIsCorrect,
            "Battle setup resolves level-scaled combat attributes");

        std::vector<autochess::core::BattleUnit> unchanged(1);
        unchanged.front().id = 99;
        const std::vector<autochess::core::UnitDefinition> noDefinitions;
        setupError.clear();
        const bool missingDefinitionCreated =
            autochess::core::BattleSetupService::createUnits(
                playerA,
                playerB,
                bundle.maps.front(),
                noDefinitions,
                bundle.factions,
                bundle.factionModifiers,
                unchanged,
                setupError);
        runner.check(
            !missingDefinitionCreated
                && !setupError.empty()
                && unchanged.size() == 1
                && unchanged.front().id == 99,
            "Battle setup rejects missing definitions atomically");

        auto invalidPlayerB = playerB;
        invalidPlayerB.side = autochess::core::MapSide::A;
        setupError.clear();
        const bool wrongSideCreated =
            autochess::core::BattleSetupService::createUnits(
                playerA,
                invalidPlayerB,
                bundle.maps.front(),
                bundle.units,
                bundle.factions,
                bundle.factionModifiers,
                unchanged,
                setupError);
        runner.check(
            !wrongSideCreated && !setupError.empty(),
            "Battle setup requires one player for each side");

        return runner.failureCount();
    }

    bool nearlyEqual(
        const double left,
        const double right,
        const double tolerance = 1.0e-9)
    {
        return std::abs(left - right) <= tolerance;
    }

    // 此函数验证等级属性和分队修正每次都从不可变定义重新计算。
    int runBattleStatResolverTests()
    {
        TestRunner runner;
        autochess::core::ConfigBundle bundle;
        autochess::core::ConfigError loadError;

        // 此代码块加载正式定义并定位要覆盖的单位和三个分队。
        const bool loaded = autochess::core::ConfigBundleLoader::load(
            AUTOCHESS_DATA_DIR,
            bundle,
            loadError);
        const autochess::core::UnitDefinition* guard = nullptr;
        const autochess::core::UnitDefinition* ranger = nullptr;
        const autochess::core::FactionDefinition* defense = nullptr;
        const autochess::core::FactionDefinition* assault = nullptr;
        const autochess::core::FactionDefinition* route = nullptr;
        for (const autochess::core::UnitDefinition& unit : bundle.units)
        {
            if (unit.id == "training_guard")
            {
                guard = &unit;
            }
            else if (unit.id == "ranger")
            {
                ranger = &unit;
            }
        }
        for (const autochess::core::FactionDefinition& faction : bundle.factions)
        {
            if (faction.id == "training_team")
            {
                defense = &faction;
            }
            else if (faction.id == "assault_team")
            {
                assault = &faction;
            }
            else if (faction.id == "route_team")
            {
                route = &faction;
            }
        }
        const bool definitionsFound = loaded
            && guard != nullptr
            && ranger != nullptr
            && defense != nullptr
            && assault != nullptr
            && route != nullptr;
        runner.check(
            definitionsFound,
            "BattleStatResolver receives units and all three factions");
        if (!definitionsFound)
        {
            return runner.failureCount();
        }

        // 此代码块验证防御分队加法和乘法修正的固定计算结果。
        autochess::core::BattleStats defenseStats;
        std::string errorMessage;
        const bool defenseResolved = autochess::core::BattleStatResolver::resolve(
            *guard,
            1,
            *defense,
            bundle.factionModifiers,
            defenseStats,
            errorMessage);
        runner.check(
            defenseResolved
                && errorMessage.empty()
                && defenseStats.maxHealth == 160.0
                && defenseStats.attackPower == 22.0
                && defenseStats.physicalDefense == 22.0,
            "BattleStatResolver applies defense faction modifiers once");

        // 此代码块再次解析相同输入，确认第一次结果不会成为第二次基础值。
        autochess::core::BattleStats repeatedStats;
        const bool repeatedResolved = autochess::core::BattleStatResolver::resolve(
            *guard,
            1,
            *defense,
            bundle.factionModifiers,
            repeatedStats,
            errorMessage);
        runner.check(
            repeatedResolved
                && repeatedStats.attackPower == defenseStats.attackPower
                && repeatedStats.physicalDefense == defenseStats.physicalDefense,
            "BattleStatResolver does not accumulate faction modifiers");

        // 此代码块验证进攻分队攻速和路线分队移速各自只作用于目标单位。
        autochess::core::BattleStats assaultStats;
        autochess::core::BattleStats routeStats;
        const bool assaultResolved = autochess::core::BattleStatResolver::resolve(
            *ranger,
            1,
            *assault,
            bundle.factionModifiers,
            assaultStats,
            errorMessage);
        const bool routeResolved = autochess::core::BattleStatResolver::resolve(
            *ranger,
            1,
            *route,
            bundle.factionModifiers,
            routeStats,
            errorMessage);
        runner.check(
            assaultResolved
                && routeResolved
                && assaultStats.attackSpeed == 130.0
                && nearlyEqual(routeStats.moveSpeed, 1.12)
                && routeStats.attackSpeed == 110.0,
            "BattleStatResolver isolates assault and route faction bonuses");

        // 此代码块覆盖通配单位修正和非法运算的原子失败语义。
        const std::vector<autochess::core::FactionModifierDefinition>
            wildcardModifier{
                {"wildcard", route->id, "*",
                    autochess::core::FactionAttribute::AttackPower,
                    autochess::core::FactionOperation::Add, 5.0}};
        autochess::core::BattleStats wildcardStats;
        const bool wildcardResolved = autochess::core::BattleStatResolver::resolve(
            *ranger,
            1,
            *route,
            wildcardModifier,
            wildcardStats,
            errorMessage);
        auto invalidModifier = wildcardModifier;
        invalidModifier.front().operation =
            autochess::core::FactionOperation::Unknown;
        autochess::core::BattleStats unchangedStats;
        unchangedStats.maxHealth = 999.0;
        const bool invalidResolved = autochess::core::BattleStatResolver::resolve(
            *ranger,
            1,
            *route,
            invalidModifier,
            unchangedStats,
            errorMessage);
        runner.check(
            wildcardResolved
                && wildcardStats.attackPower == 33.0
                && !invalidResolved
                && !errorMessage.empty()
                && unchangedStats.maxHealth == 999.0,
            "BattleStatResolver supports wildcard units and rejects invalid operations");

        return runner.failureCount();
    }

    int runCombatRulesTests()
    {
        TestRunner runner;

        runner.check(
            nearlyEqual(
                autochess::core::CombatRules::attackInterval(0.0),
                2.0),
            "Combat rules calculate the zero-speed attack interval");
        runner.check(
            nearlyEqual(
                autochess::core::CombatRules::attackInterval(600.0),
                200.0 / 700.0),
            "Combat rules calculate the maximum-speed attack interval");
        runner.check(
            nearlyEqual(
                autochess::core::CombatRules::attackInterval(900.0),
                200.0 / 700.0),
            "Combat rules clamp attack speed above 600");
        runner.check(
            nearlyEqual(
                autochess::core::CombatRules::physicalDamage(20.0, 7.0),
                13.0),
            "Combat rules subtract physical defense");
        runner.check(
            nearlyEqual(
                autochess::core::CombatRules::physicalDamage(20.0, 100.0),
                5.0),
            "Combat rules enforce five minimum physical damage");
        runner.check(
            nearlyEqual(
                autochess::core::CombatRules::magicDamage(20.0, 0.0),
                20.0)
                && nearlyEqual(
                    autochess::core::CombatRules::magicDamage(20.0, 100.0),
                    0.0),
            "Combat rules apply the magic-resistance boundaries");
        runner.check(
            nearlyEqual(
                autochess::core::CombatRules::distance(
                    {1.0, 1.0},
                    {4.0, 5.0}),
                5.0),
            "Combat rules use Euclidean distance");

        return runner.failureCount();
    }

    int runBattleMovementTests()
    {
        TestRunner runner;

        autochess::core::MapDefinition map;
        map.id = "movement_map";
        map.width = 7;
        map.height = 4;
        map.gridRows = {
            "#######",
            "#.....#",
            "#.....#",
            "#######"};

        auto makeUnit = []()
        {
            autochess::core::BattleUnit unit;
            unit.id = 1;
            unit.ownedUnitId = 10;
            unit.identity = {"movement_unit", 1};
            unit.side = autochess::core::MapSide::A;
            unit.position = {1.5, 1.5};
            unit.routePoints = {
                {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}};
            unit.nextRoutePointIndex = 1;
            unit.stats.maxHealth = 100.0;
            unit.health = 100.0;
            unit.stats.moveSpeed = 30.0;
            unit.stats.guardDamage = 5;
            return unit;
        };

        {
            auto unit = makeUnit();
            autochess::core::BattleSimulation simulation({unit}, map, 10);
            const bool stepped = simulation.step();
            const auto& moved = simulation.units().front();
            runner.check(
                stepped
                    && simulation.currentFrame() == 1
                    && nearlyEqual(moved.position.x, 2.0)
                    && nearlyEqual(moved.position.y, 1.5)
                    && moved.nextRoutePointIndex == 1,
                "Battle movement advances by speed divided by sixty");
        }

        {
            auto unit = makeUnit();
            unit.stats.moveSpeed = 150.0;
            autochess::core::BattleSimulation simulation({unit}, map, 10);
            simulation.step();
            const auto& moved = simulation.units().front();
            runner.check(
                nearlyEqual(moved.position.x, 4.0)
                    && nearlyEqual(moved.position.y, 1.5)
                    && moved.nextRoutePointIndex == 3,
                "Battle movement consumes distance across multiple waypoints");
        }

        {
            auto unit = makeUnit();
            unit.position = {3.5, 2.5};
            unit.stats.moveSpeed = 60.0;
            autochess::core::BattleSimulation simulation({unit}, map, 10);
            simulation.step();
            const auto& recovered = simulation.units().front();
            runner.check(
                recovered.nextRoutePointIndex == 3
                    && nearlyEqual(recovered.position.x, 3.5)
                    && nearlyEqual(recovered.position.y, 1.5),
                "Battle movement recovers to the nearest unvisited waypoint");
        }

        {
            auto unit = makeUnit();
            unit.stats.moveSpeed = 240.0;
            autochess::core::BattleSimulation simulation({unit}, map, 10);
            simulation.step();
            const auto& reached = simulation.units().front();
            runner.check(
                simulation.isFinished()
                    && reached.state
                        == autochess::core::BattleUnitState::ReachedGuard
                    && nearlyEqual(reached.position.x, 5.5)
                    && simulation.summary().guardDamageToB == 5,
                "Battle movement stops exactly at the guard endpoint");
        }

        {
            auto blockedMap = map;
            blockedMap.gridRows[1] = "#.#...#";
            auto unit = makeUnit();
            unit.stats.moveSpeed = 60.0;
            autochess::core::BattleSimulation simulation(
                {unit},
                blockedMap,
                10);
            simulation.step();
            const auto& blocked = simulation.units().front();
            runner.check(
                blocked.position
                    == autochess::core::BattlePosition{1.5, 1.5}
                    && blocked.state
                        == autochess::core::BattleUnitState::Alive,
                "Battle movement does not enter an obstacle cell");
        }

        return runner.failureCount();
    }

    int runTargetSelectorTests()
    {
        TestRunner runner;

        autochess::core::BattleUnit attacker;
        attacker.id = 1;
        // 此字段让既有索敌案例继续验证普通攻击目标规则。
        attacker.basicAction = autochess::core::BasicAction::Attack;
        attacker.side = autochess::core::MapSide::A;
        attacker.position = {1.5, 1.5};
        attacker.stats.attackRange = 2.0;

        auto firstTarget = attacker;
        firstTarget.id = 2;
        firstTarget.ownedUnitId = 20;
        firstTarget.side = autochess::core::MapSide::B;
        firstTarget.position = {4.5, 1.5};

        auto secondTarget = firstTarget;
        secondTarget.id = 3;
        secondTarget.ownedUnitId = 30;
        secondTarget.position = {2.5, 1.5};

        std::vector<autochess::core::BattleUnit> units = {
            attacker,
            firstTarget,
            secondTarget};
        autochess::core::TargetSelector::update(units[0], units, 5);
        runner.check(
            units[0].targetId.has_value()
                && units[0].targetId.value() == 3
                && units[0].firstInRangeFrame.size() == 1
                && units[0].firstInRangeFrame.at(3) == 5,
            "Target selector records the first frame in range");

        units[1].position = {2.5, 1.5};
        autochess::core::TargetSelector::update(units[0], units, 6);
        runner.check(
            units[0].targetId.has_value()
                && units[0].targetId.value() == 3
                && units[0].firstInRangeFrame.at(2) == 6
                && units[0].firstInRangeFrame.at(3) == 5,
            "Target selector prefers the earlier entering target");

        units[2].position = {4.5, 1.5};
        autochess::core::TargetSelector::update(units[0], units, 7);
        runner.check(
            units[0].targetId.has_value()
                && units[0].targetId.value() == 2
                && units[0].firstInRangeFrame.size() == 1
                && units[0].firstInRangeFrame.at(2) == 6,
            "Target selector clears an out-of-range target and reacquires");

        units[1].state = autochess::core::BattleUnitState::Dead;
        autochess::core::TargetSelector::update(units[0], units, 8);
        runner.check(
            !units[0].targetId.has_value()
                && units[0].firstInRangeFrame.empty(),
            "Target selector clears dead targets immediately");

        // 此代码块验证普通治疗按最低生命比例和编号选择同阵营目标。
        auto healer = attacker;
        healer.id = 5;
        healer.basicAction = autochess::core::BasicAction::Heal;
        healer.side = autochess::core::MapSide::A;
        healer.health = 50.0;
        healer.stats.maxHealth = 100.0;
        healer.stats.attackRange = 3.0;
        auto lowerIdAlly = healer;
        lowerIdAlly.id = 2;
        lowerIdAlly.health = 20.0;
        auto higherIdAlly = healer;
        higherIdAlly.id = 3;
        higherIdAlly.health = 40.0;
        higherIdAlly.stats.maxHealth = 200.0;
        auto fullAlly = healer;
        fullAlly.id = 4;
        fullAlly.health = 100.0;
        auto enemy = lowerIdAlly;
        enemy.id = 1;
        enemy.side = autochess::core::MapSide::B;
        std::vector<autochess::core::BattleUnit> healingUnits = {
            enemy, lowerIdAlly, higherIdAlly, fullAlly, healer};
        autochess::core::TargetSelector::update(
            healingUnits[4], healingUnits, 9);
        runner.check(
            healingUnits[4].targetId.has_value()
                && healingUnits[4].targetId.value() == 2
                && healingUnits[4].firstInRangeFrame.empty(),
            "Target selector heals the lowest-ratio ally with ID tie-break");

        return runner.failureCount();
    }

    int runBattleAttackTests()
    {
        TestRunner runner;

        auto makeUnit = [](
            const autochess::core::BattleUnitId id,
            const autochess::core::OwnedUnitId ownedUnitId,
            const autochess::core::MapSide side,
            const double health,
            const double attackPower)
        {
            autochess::core::BattleUnit unit;
            unit.id = id;
            unit.ownedUnitId = ownedUnitId;
            unit.identity = {"attack_unit", 1};
            unit.side = side;
            unit.basicAction = autochess::core::BasicAction::Attack;
            unit.basicDamageType = autochess::core::DamageType::Physical;
            unit.position = side == autochess::core::MapSide::A
                ? autochess::core::BattlePosition{1.5, 1.5}
                : autochess::core::BattlePosition{2.5, 1.5};
            unit.stats.maxHealth = health;
            unit.stats.attackPower = attackPower;
            unit.stats.physicalDefense = 7.0;
            unit.stats.attackRange = 2.0;
            unit.stats.attackSpeed = 100.0;
            unit.health = health;
            return unit;
        };

        autochess::core::MapDefinition map;
        map.id = "attack_map";
        map.width = 6;
        map.height = 3;
        map.gridRows = {
            "######",
            "#....#",
            "######"};

        {
            auto attacker = makeUnit(1, 10, autochess::core::MapSide::A,
                100.0, 20.0);
            auto target = makeUnit(2, 20, autochess::core::MapSide::B,
                20.0, 0.0);
            target.basicAction = autochess::core::BasicAction::Unknown;
            target.stats.attackRange = 0.0;

            autochess::core::BattleSimulation simulation(
                {attacker, target},
                map,
                3);
            for (int frame = 0; frame < 60; ++frame)
            {
                simulation.step();
            }

            runner.check(
                !simulation.isFinished()
                    && nearlyEqual(simulation.units()[1].health, 7.0)
                    && simulation.units()[0].targetId.has_value()
                    && simulation.units()[0].targetId.value() == 2,
                "Battle attacks apply physical damage after one interval");
        }

        {
            auto first = makeUnit(1, 10, autochess::core::MapSide::A,
                5.0, 0.0);
            auto second = makeUnit(2, 20, autochess::core::MapSide::B,
                5.0, 0.0);
            autochess::core::BattleSimulation simulation(
                {first, second},
                map,
                3);
            for (int frame = 0; frame < 60; ++frame)
            {
                simulation.step();
            }

            const auto& summary = simulation.summary();
            runner.check(
                simulation.isFinished()
                    && summary.endReason
                        == autochess::core::BattleSummary::EndReason::
                            AllUnitsResolved
                    && simulation.units()[0].state
                        == autochess::core::BattleUnitState::Dead
                    && simulation.units()[1].state
                        == autochess::core::BattleUnitState::Dead
                    && summary.deadOwnedUnitIds
                        == std::vector<autochess::core::OwnedUnitId>{10, 20},
                "Battle attacks resolve simultaneous mutual deaths in one frame");
        }

        return runner.failureCount();
    }

    // 此函数验证普通治疗、自疗、上限和同帧净生命结算。
    int runBattleHealingTests()
    {
        TestRunner runner;

        autochess::core::MapDefinition map;
        map.id = "healing_map";
        map.width = 6;
        map.height = 3;
        map.gridRows = {
            "######",
            "#....#",
            "######"};

        // 此辅助代码块创建无需路线移动的固定位置战斗单位。
        const auto makeUnit = [](
            const autochess::core::BattleUnitId id,
            const autochess::core::MapSide side,
            const autochess::core::BattlePosition position,
            const double health)
        {
            autochess::core::BattleUnit unit;
            unit.id = id;
            unit.ownedUnitId = id * 10;
            unit.identity = {"healing_unit", 1};
            unit.side = side;
            unit.position = position;
            unit.stats.maxHealth = 100.0;
            unit.stats.physicalDefense = 7.0;
            unit.stats.attackRange = 2.0;
            unit.stats.attackSpeed = 0.0;
            unit.health = health;
            return unit;
        };

        // 此代码块验证医师在两秒间隔后治疗最低血量友军并限制到满血。
        auto healer = makeUnit(
            1, autochess::core::MapSide::A, {1.5, 1.5}, 100.0);
        healer.basicAction = autochess::core::BasicAction::Heal;
        healer.basicDamageType = autochess::core::DamageType::None;
        healer.stats.attackPower = 16.0;
        auto wounded = makeUnit(
            2, autochess::core::MapSide::A, {2.5, 1.5}, 90.0);
        autochess::core::BattleSimulation healingSimulation(
            {healer, wounded}, map, 3);
        for (int frame = 0; frame < 120; ++frame)
        {
            healingSimulation.step();
        }
        runner.check(
            nearlyEqual(healingSimulation.units()[1].health, 100.0)
                && healingSimulation.units()[0].targetId.has_value()
                && healingSimulation.units()[0].targetId.value() == 2,
            "Battle healing restores an ally without exceeding max health");

        // 此代码块验证没有其他受伤友军时医师能够选择并治疗自己。
        auto selfHealer = healer;
        selfHealer.health = 50.0;
        auto fullAlly = wounded;
        fullAlly.health = 100.0;
        autochess::core::BattleSimulation selfHealingSimulation(
            {selfHealer, fullAlly}, map, 3);
        for (int frame = 0; frame < 120; ++frame)
        {
            selfHealingSimulation.step();
        }
        runner.check(
            nearlyEqual(selfHealingSimulation.units()[0].health, 66.0),
            "Battle healing permits a healer to heal itself");

        // 此代码块验证同帧伤害和治疗按净生命变化结算而不依赖遍历顺序。
        auto netHealer = healer;
        netHealer.position = {1.5, 1.5};
        auto fragileAlly = wounded;
        fragileAlly.position = {2.5, 1.5};
        fragileAlly.health = 10.0;
        auto enemyAttacker = makeUnit(
            3, autochess::core::MapSide::B, {3.0, 1.5}, 100.0);
        enemyAttacker.basicAction = autochess::core::BasicAction::Attack;
        enemyAttacker.basicDamageType = autochess::core::DamageType::Physical;
        enemyAttacker.stats.attackPower = 20.0;
        enemyAttacker.stats.attackRange = 0.6;
        autochess::core::BattleSimulation netSimulation(
            {netHealer, fragileAlly, enemyAttacker}, map, 3);
        for (int frame = 0; frame < 120; ++frame)
        {
            netSimulation.step();
        }
        runner.check(
            netSimulation.units()[1].state
                    == autochess::core::BattleUnitState::Alive
                && nearlyEqual(netSimulation.units()[1].health, 13.0),
            "Battle batches same-frame damage and healing as one net change");

        return runner.failureCount();
    }

    // 此函数验证技能释放、技力恢复和持续行为许可已接入固定步长战斗。
    int runBattleSkillFlowTests()
    {
        TestRunner runner;

        autochess::core::MapDefinition map;
        map.id = "skill_flow_map";
        map.width = 6;
        map.height = 3;
        map.gridRows = {
            "######",
            "#....#",
            "######"};

        // 此辅助代码块创建基础属性与实时属性一致的满血技能战斗单位。
        const auto makeUnit = [](
            const autochess::core::BattleUnitId id,
            const autochess::core::MapSide side,
            const autochess::core::BattlePosition position)
        {
            autochess::core::BattleUnit unit;
            unit.id = id;
            unit.ownedUnitId = id * 10;
            unit.identity = {"skill_flow_unit", 1};
            unit.side = side;
            unit.position = position;
            unit.baseStats.maxHealth = 100.0;
            unit.baseStats.attackPower = 20.0;
            unit.baseStats.physicalDefense = 5.0;
            unit.baseStats.magicResistance = 20.0;
            unit.baseStats.attackRange = 2.0;
            unit.baseStats.moveSpeed = 60.0;
            unit.baseStats.attackSpeed = 0.0;
            unit.stats = unit.baseStats;
            unit.health = unit.stats.maxHealth;
            unit.maxMana = 10.0;
            return unit;
        };

        // 此代码块验证固定六十帧使战斗单位准确恢复一点技力。
        auto manaUnit = makeUnit(
            1, autochess::core::MapSide::A, {1.5, 1.5});
        autochess::core::BattleSimulation manaSimulation(
            {manaUnit}, map, 3);
        for (int frame = 0; frame < 60; ++frame)
        {
            manaSimulation.step();
        }
        runner.check(
            nearlyEqual(manaSimulation.units()[0].currentMana, 1.0),
            "Battle fixed steps regenerate exactly one mana per second");

        // 此代码块验证战斗释放入口查找技能定义、自动选敌并应用法术伤害。
        auto damageCaster = makeUnit(
            2, autochess::core::MapSide::A, {1.5, 1.5});
        damageCaster.skillId = "flow_damage";
        damageCaster.currentMana = damageCaster.maxMana;
        auto damageTarget = makeUnit(
            3, autochess::core::MapSide::B, {2.5, 1.5});
        autochess::core::SkillDefinition damageSkill;
        damageSkill.id = "flow_damage";
        damageSkill.effectType = autochess::core::SkillEffectType::Damage;
        damageSkill.targetRule = autochess::core::SkillTargetRule::Enemy;
        damageSkill.targetCount = 1;
        damageSkill.effectRange = 2.0;
        damageSkill.damageType = autochess::core::DamageType::Magic;
        damageSkill.levelValues = {20.0, 30.0, 45.0};
        autochess::core::BattleSimulation damageSimulation(
            {damageCaster, damageTarget}, map, 3, {damageSkill});
        const bool damageReleased = damageSimulation.releaseSkill(2);
        runner.check(
            damageReleased
                && damageSimulation.units()[0].currentMana == 0.0
                && nearlyEqual(damageSimulation.units()[1].health, 84.0)
                && !damageSimulation.releaseSkill(2),
            "Battle release applies configured skill and rejects empty mana");

        // 此代码块验证技能生效帧禁止移动和普通行动且不恢复技力。
        auto blockedCaster = makeUnit(
            4, autochess::core::MapSide::A, {1.5, 1.5});
        blockedCaster.skillId = "flow_block";
        blockedCaster.currentMana = blockedCaster.maxMana;
        blockedCaster.basicAction = autochess::core::BasicAction::Attack;
        blockedCaster.basicDamageType = autochess::core::DamageType::Physical;
        blockedCaster.routePoints = {{1, 1}, {4, 1}};
        blockedCaster.nextRoutePointIndex = 1;
        autochess::core::SkillDefinition blockedSkill;
        blockedSkill.id = "flow_block";
        blockedSkill.effectType = autochess::core::SkillEffectType::Buff;
        blockedSkill.targetRule = autochess::core::SkillTargetRule::Self;
        blockedSkill.targetCount = 1;
        blockedSkill.effectRange = 0.0;
        blockedSkill.levelValues = {0.0, 0.0, 0.0};
        blockedSkill.durationSeconds =
            autochess::core::BattleSimulation::FixedDeltaSeconds;
        blockedSkill.buffStat = autochess::core::BuffStat::MoveSpeed;
        blockedSkill.modifierMode = autochess::core::ModifierMode::Add;
        blockedSkill.allowSelf = true;
        blockedSkill.allowMove = false;
        blockedSkill.allowBasicAction = false;
        autochess::core::BattleSimulation blockedSimulation(
            {blockedCaster}, map, 3, {blockedSkill});
        const bool blockedReleased = blockedSimulation.releaseSkill(4);
        blockedSimulation.step();
        const bool blockedFrameValid = blockedReleased
            && nearlyEqual(blockedSimulation.units()[0].position.x, 1.5)
            && blockedSimulation.units()[0].basicActionElapsed == 0.0
            && blockedSimulation.units()[0].currentMana == 0.0
            && !blockedSimulation.units()[0].activeSkill.active;
        blockedSimulation.step();
        runner.check(
            blockedFrameValid
                && nearlyEqual(
                    blockedSimulation.units()[0].position.x,
                    2.5)
                && nearlyEqual(
                    blockedSimulation.units()[0].basicActionElapsed,
                    autochess::core::BattleSimulation::FixedDeltaSeconds)
                && nearlyEqual(
                    blockedSimulation.units()[0].currentMana,
                    autochess::core::BattleSimulation::FixedDeltaSeconds),
            "Battle resumes movement, action timing, and mana next frame");

        return runner.failureCount();
    }

    // 此函数用正式配置验证五个单位技能矩阵和分队属性不累积。
    int runDay5SkillMatrixTests()
    {
        TestRunner runner;
        autochess::core::ConfigBundle bundle;
        autochess::core::ConfigError loadError;
        const bool loaded = autochess::core::ConfigBundleLoader::load(
            AUTOCHESS_DATA_DIR,
            bundle,
            loadError);

        // 此辅助代码块按正式 ID 查找单位和分队定义供矩阵逐项创建施法者。
        const auto findUnitDefinition = [&bundle](const std::string& id)
        {
            const auto iterator = std::find_if(
                bundle.units.begin(),
                bundle.units.end(),
                [&id](const autochess::core::UnitDefinition& definition)
                {
                    return definition.id == id;
                });
            return iterator == bundle.units.end() ? nullptr : &*iterator;
        };
        const auto findFactionDefinition = [&bundle](const std::string& id)
        {
            const auto iterator = std::find_if(
                bundle.factions.begin(),
                bundle.factions.end(),
                [&id](const autochess::core::FactionDefinition& definition)
                {
                    return definition.id == id;
                });
            return iterator == bundle.factions.end() ? nullptr : &*iterator;
        };

        const auto* guardDefinition = findUnitDefinition("training_guard");
        const auto* duelistDefinition = findUnitDefinition("duelist");
        const auto* rangerDefinition = findUnitDefinition("ranger");
        const auto* arcanistDefinition = findUnitDefinition("arcanist");
        const auto* medicDefinition = findUnitDefinition("medic");
        const auto* defenseFaction = findFactionDefinition("training_team");
        const auto* assaultFaction = findFactionDefinition("assault_team");
        const auto* routeFaction = findFactionDefinition("route_team");
        const bool definitionsFound = loaded
            && !bundle.maps.empty()
            && guardDefinition != nullptr
            && duelistDefinition != nullptr
            && rangerDefinition != nullptr
            && arcanistDefinition != nullptr
            && medicDefinition != nullptr
            && defenseFaction != nullptr
            && assaultFaction != nullptr
            && routeFaction != nullptr;
        runner.check(
            definitionsFound,
            "Day 5 skill matrix finds all formal units and factions");
        if (!definitionsFound)
        {
            return runner.failureCount();
        }

        // 此辅助代码块从正式单位、等级和分队定义创建满技力施法者。
        const auto prepareCaster = [&bundle](
            const autochess::core::UnitDefinition& definition,
            const autochess::core::FactionDefinition& faction,
            const autochess::core::BattleUnitId id,
            autochess::core::BattleUnit& output)
        {
            output = autochess::core::BattleUnit{};
            output.id = id;
            output.ownedUnitId = id * 10;
            output.identity = {definition.id, 1};
            output.side = autochess::core::MapSide::A;
            output.basicAction = definition.basicAction;
            output.basicDamageType = definition.basicDamageType;
            output.skillId = definition.skillId;
            output.currentMana = static_cast<double>(definition.maxMana);
            output.maxMana = static_cast<double>(definition.maxMana);
            output.position = {2.5, 3.5};
            std::string errorMessage;
            if (!autochess::core::BattleStatResolver::resolve(
                    definition,
                    1,
                    faction,
                    bundle.factionModifiers,
                    output.stats,
                    errorMessage))
            {
                return false;
            }
            output.baseStats = output.stats;
            output.health = output.stats.maxHealth;
            return true;
        };

        // 此辅助代码块创建具有固定防御和法抗的技能目标用于数值断言。
        const auto makeTarget = [](
            const autochess::core::BattleUnitId id,
            const autochess::core::MapSide side,
            const autochess::core::BattlePosition position,
            const double health)
        {
            autochess::core::BattleUnit target;
            target.id = id;
            target.ownedUnitId = id * 10;
            target.identity = {"matrix_target", 1};
            target.side = side;
            target.position = position;
            target.baseStats.maxHealth = 100.0;
            target.baseStats.physicalDefense = 7.0;
            target.baseStats.magicResistance = 20.0;
            target.stats = target.baseStats;
            target.health = health;
            return target;
        };

        autochess::core::BattleUnit guard;
        autochess::core::BattleUnit duelist;
        autochess::core::BattleUnit ranger;
        autochess::core::BattleUnit arcanist;
        autochess::core::BattleUnit medic;
        const bool castersPrepared = prepareCaster(
                *guardDefinition, *defenseFaction, 10, guard)
            && prepareCaster(
                *duelistDefinition, *assaultFaction, 20, duelist)
            && prepareCaster(
                *rangerDefinition, *assaultFaction, 30, ranger)
            && prepareCaster(
                *arcanistDefinition, *routeFaction, 40, arcanist)
            && prepareCaster(
                *medicDefinition, *routeFaction, 50, medic);
        runner.check(
            castersPrepared,
            "Day 5 skill matrix resolves all faction battle stats");
        if (!castersPrepared)
        {
            return runner.failureCount();
        }

        // 此代码块验证铁卫增益结束和回满技力后再次释放仍从分队基础防御计算。
        autochess::core::BattleSimulation guardSimulation(
            {guard}, bundle.maps.front(), 60, bundle.skills);
        const bool firstGuardRelease = guardSimulation.releaseSkill(10);
        const bool firstGuardValue = firstGuardRelease
            && guardSimulation.units()[0].baseStats.physicalDefense == 22.0
            && guardSimulation.units()[0].stats.physicalDefense == 30.0;
        for (int frame = 0; frame < 180; ++frame)
        {
            guardSimulation.step();
        }
        const bool guardRestored =
            guardSimulation.units()[0].stats.physicalDefense == 22.0
            && !guardSimulation.units()[0].activeSkill.active;
        for (int frame = 0; frame < 600; ++frame)
        {
            guardSimulation.step();
        }
        const bool secondGuardRelease = guardSimulation.releaseSkill(10);
        runner.check(
            firstGuardValue
                && guardRestored
                && secondGuardRelease
                && guardSimulation.units()[0].stats.physicalDefense == 30.0,
            "Iron guard buff preserves faction base stats without accumulation");

        // 此代码块验证决斗者正式技能造成一次配置驱动的单体物理伤害。
        auto duelistTarget = makeTarget(
            21, autochess::core::MapSide::B, {3.5, 3.5}, 100.0);
        autochess::core::BattleSimulation duelistSimulation(
            {duelist, duelistTarget},
            bundle.maps.front(),
            60,
            bundle.skills);
        runner.check(
            nearlyEqual(duelist.baseStats.attackPower, 35.84)
                && duelistSimulation.releaseSkill(20)
                && duelistSimulation.units()[1].health == 85.0,
            "Duelist releases its formal single-target physical skill");

        // 此代码块验证游侠正式技能从突击分队基础攻速计算乘法增益。
        autochess::core::BattleSimulation rangerSimulation(
            {ranger}, bundle.maps.front(), 60, bundle.skills);
        runner.check(
            ranger.baseStats.attackSpeed == 130.0
                && rangerSimulation.releaseSkill(30)
                && nearlyEqual(
                    rangerSimulation.units()[0].stats.attackSpeed,
                    149.5),
            "Ranger releases its formal multiplicative speed buff");

        // 此代码块验证奥术师正式技能只伤害范围内最近的三个敌方单位。
        auto arcaneTargetA = makeTarget(
            41, autochess::core::MapSide::B, {3.5, 3.5}, 100.0);
        auto arcaneTargetB = makeTarget(
            42, autochess::core::MapSide::B, {4.5, 3.5}, 100.0);
        auto arcaneTargetC = makeTarget(
            43, autochess::core::MapSide::B, {5.5, 3.5}, 100.0);
        auto arcaneTargetD = makeTarget(
            44, autochess::core::MapSide::B, {6.5, 3.5}, 100.0);
        autochess::core::BattleSimulation arcanistSimulation(
            {arcanist,
             arcaneTargetD,
             arcaneTargetB,
             arcaneTargetA,
             arcaneTargetC},
            bundle.maps.front(),
            60,
            bundle.skills);
        const bool arcanistReleased = arcanistSimulation.releaseSkill(40);
        runner.check(
            arcanistReleased
                && arcanistSimulation.units()[1].health == 100.0
                && nearlyEqual(arcanistSimulation.units()[2].health, 85.6)
                && nearlyEqual(arcanistSimulation.units()[3].health, 85.6)
                && nearlyEqual(arcanistSimulation.units()[4].health, 85.6),
            "Arcanist releases its formal three-target magic skill");

        // 此代码块验证医师正式技能按生命比例和编号治疗一个受伤友军。
        auto medicHigherId = makeTarget(
            52, autochess::core::MapSide::A, {3.5, 3.5}, 20.0);
        auto medicLowerId = medicHigherId;
        medicLowerId.id = 51;
        medicLowerId.ownedUnitId = 510;
        autochess::core::BattleSimulation medicSimulation(
            {medic, medicHigherId, medicLowerId},
            bundle.maps.front(),
            60,
            bundle.skills);
        const bool medicReleased = medicSimulation.releaseSkill(50);
        runner.check(
            medicReleased
                && medicSimulation.units()[1].health == 20.0
                && medicSimulation.units()[2].health == 44.0,
            "Medic releases its formal lowest-health ally skill");

        return runner.failureCount();
    }

    int runBattleEndConditionTests()
    {
        TestRunner runner;

        autochess::core::MapDefinition map;
        map.id = "end_condition_map";
        map.width = 6;
        map.height = 3;
        map.gridRows = {
            "######",
            "#....#",
            "######"};

        auto makeArrivingUnit = [](
            const autochess::core::BattleUnitId id,
            const autochess::core::OwnedUnitId ownedId,
            const autochess::core::MapSide side,
            const double startX,
            const int guardDamage)
        {
            autochess::core::BattleUnit unit;
            unit.id = id;
            unit.ownedUnitId = ownedId;
            unit.side = side;
            unit.position = {startX, 1.5};
            unit.routePoints = side == autochess::core::MapSide::A
                ? std::vector<autochess::core::GridPosition>{{1, 1}, {2, 1}}
                : std::vector<autochess::core::GridPosition>{{3, 1}, {2, 1}};
            unit.nextRoutePointIndex = 1;
            unit.stats.maxHealth = 100.0;
            unit.health = 100.0;
            unit.stats.moveSpeed = 60.0;
            unit.stats.guardDamage = guardDamage;
            return unit;
        };

        {
            const auto first = makeArrivingUnit(
                1,
                10,
                autochess::core::MapSide::A,
                1.5,
                5);
            const auto second = makeArrivingUnit(
                2,
                20,
                autochess::core::MapSide::B,
                3.5,
                7);
            autochess::core::BattleSimulation simulation(
                {first, second},
                map,
                10);
            const bool stepped = simulation.step();
            const auto& summary = simulation.summary();
            runner.check(
                stepped
                    && simulation.isFinished()
                    && summary.endReason
                        == autochess::core::BattleSummary::EndReason::
                            AllUnitsResolved
                    && summary.elapsedFrames == 1
                    && summary.guardDamageToA == 7
                    && summary.guardDamageToB == 5
                    && summary.reachedGuardOwnedUnitIds
                        == std::vector<autochess::core::OwnedUnitId>{10, 20}
                    && summary.survivingOwnedUnitIds.empty(),
                "Battle batches simultaneous guard arrivals and damage");
        }

        {
            auto waiting = makeArrivingUnit(
                1,
                10,
                autochess::core::MapSide::A,
                1.5,
                5);
            waiting.routePoints.clear();
            waiting.stats.moveSpeed = 0.0;
            autochess::core::BattleSimulation simulation(
                {waiting},
                map,
                1);
            for (int frame = 0; frame < 60; ++frame)
            {
                simulation.step();
            }

            const auto frameAtTimeout = simulation.currentFrame();
            const bool extraStep = simulation.step();
            const auto& summary = simulation.summary();
            runner.check(
                simulation.isFinished()
                    && !extraStep
                    && frameAtTimeout == 60
                    && summary.endReason
                        == autochess::core::BattleSummary::EndReason::Timeout
                    && summary.elapsedFrames == 60
                    && summary.survivingOwnedUnitIds
                        == std::vector<autochess::core::OwnedUnitId>{10}
                    && simulation.currentFrame() == frameAtTimeout,
                "Battle timeout preserves remaining units and is idempotent");
        }

        {
            auto moving = makeArrivingUnit(
                1,
                10,
                autochess::core::MapSide::A,
                1.5,
                5);
            moving.routePoints = {
                {1, 1}, {2, 1}, {3, 1}, {4, 1}};
            moving.stats.moveSpeed = 1.0;
            moving.stats.attackRange = 1.1;
            moving.basicAction = autochess::core::BasicAction::Attack;
            moving.basicDamageType = autochess::core::DamageType::Physical;
            moving.stats.attackPower = 20.0;

            auto fleeing = makeArrivingUnit(
                2,
                20,
                autochess::core::MapSide::B,
                2.5,
                0);
            fleeing.routePoints = {
                {2, 1}, {3, 1}, {4, 1}};
            fleeing.stats.moveSpeed = 1.0;
            fleeing.stats.attackRange = 0.0;
            fleeing.basicAction = autochess::core::BasicAction::Unknown;

            autochess::core::BattleSimulation simulation(
                {moving, fleeing},
                map,
                3);
            for (int frame = 0; frame < 10; ++frame)
            {
                simulation.step();
            }

            const double positionBeforeResume = simulation.units()[0].position.x;
            const bool hadNoTarget = !simulation.units()[0].targetId.has_value();
            simulation.step();
            runner.check(
                hadNoTarget
                    && simulation.units()[0].position.x > positionBeforeResume,
                "Battle resumes route movement after a target leaves range");
        }

        return runner.failureCount();
    }
}

// 此函数依次运行所有核心配置测试并把失败转换为非零退出码。
int main()
{
    constexpr int expectedValue = 42;
    const int actualValue = autochess::core::sanityCheckValue();

    assert(actualValue == expectedValue);

    if (actualValue != expectedValue)
    {
        std::cerr
            << "[FAIL] AutoChessCore sanity check: expected "
            << expectedValue
            << ", actual "
            << actualValue
            << '\n';

        return 1;
    }

    std::cout << "[PASS] AutoChessCore sanity check\n";

    const bool typesPassed = runCoreTypesSmokeTest();

    assert(typesPassed);

    if (!typesPassed)
    {
        std::cerr << "[FAIL] AutoChessCore data types smoke test\n";
        return 1;
    }

    std::cout << "[PASS] AutoChessCore data types smoke test\n";

    const bool playerTypesPassed = runPlayerTypesSmokeTest();

    assert(playerTypesPassed);

    if (!playerTypesPassed)
    {
        std::cerr << "[FAIL] Player state data types smoke test\n";
        return 1;
    }

    std::cout << "[PASS] Player state data types smoke test\n";

    const bool economyTypesPassed = runEconomyTypesSmokeTest();

    assert(economyTypesPassed);

    if (!economyTypesPassed)
    {
        std::cerr << "[FAIL] Economy data types smoke test\n";
        return 1;
    }

    std::cout << "[PASS] Economy data types smoke test\n";

    const int priceRulesFailures = runPriceRulesTests();
    assert(priceRulesFailures == 0);
    if (priceRulesFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Price rules test suite\n";

    const int shopServiceFailures = runShopServiceTests();
    assert(shopServiceFailures == 0);
    if (shopServiceFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Shop service test suite\n";

    const int shopPurchaseFailures = runShopPurchaseTests();
    assert(shopPurchaseFailures == 0);
    if (shopPurchaseFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Shop purchase test suite\n";

    const int playerStateServiceFailures = runPlayerStateServiceTests();
    assert(playerStateServiceFailures == 0);
    if (playerStateServiceFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] PlayerState service test suite\n";

    const int deploymentServiceFailures = runDeploymentServiceTests();
    assert(deploymentServiceFailures == 0);
    if (deploymentServiceFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Deployment service test suite\n";

    // 此代码段运行合成服务测试并将任一失败转换为非零退出码。
    const int mergeServiceFailures = runMergeServiceTests();
    assert(mergeServiceFailures == 0);
    if (mergeServiceFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Merge service test suite\n";

    // 此代码段运行出售、死亡转移和复活测试并传播失败退出码。
    const int rosterServiceFailures = runRosterServiceTests();
    assert(rosterServiceFailures == 0);
    if (rosterServiceFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Roster service test suite\n";

    const int economyIntegrationFailures =
        runEconomyIntegrationScenarioTests();
    assert(economyIntegrationFailures == 0);
    if (economyIntegrationFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Economy integration scenario test suite\n";

    // 此代码块运行分队战斗属性解析测试并传播任一失败。
    const int battleStatResolverFailures = runBattleStatResolverTests();
    assert(battleStatResolverFailures == 0);
    if (battleStatResolverFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Battle stat resolver test suite\n";

    const int battleSetupFailures = runBattleSetupServiceTests();
    assert(battleSetupFailures == 0);
    if (battleSetupFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Battle setup service test suite\n";

    const int combatRulesFailures = runCombatRulesTests();
    assert(combatRulesFailures == 0);
    if (combatRulesFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Combat rules test suite\n";

    const int battleMovementFailures = runBattleMovementTests();
    assert(battleMovementFailures == 0);
    if (battleMovementFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Battle movement test suite\n";

    const int targetSelectorFailures = runTargetSelectorTests();
    assert(targetSelectorFailures == 0);
    if (targetSelectorFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Target selector test suite\n";

    const int battleAttackFailures = runBattleAttackTests();
    assert(battleAttackFailures == 0);
    if (battleAttackFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Battle attack test suite\n";

    // 此代码块运行普通治疗测试并传播任一失败。
    const int battleHealingFailures = runBattleHealingTests();
    assert(battleHealingFailures == 0);
    if (battleHealingFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Battle healing test suite\n";

    // 此代码块运行固定步长技能流程测试并传播任一失败。
    const int battleSkillFlowFailures = runBattleSkillFlowTests();
    assert(battleSkillFlowFailures == 0);
    if (battleSkillFlowFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Battle skill flow test suite\n";

    // 此代码块运行五单位正式技能矩阵并传播任一失败。
    const int day5SkillMatrixFailures = runDay5SkillMatrixTests();
    assert(day5SkillMatrixFailures == 0);
    if (day5SkillMatrixFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Day 5 skill matrix test suite\n";

    const int battleEndConditionFailures = runBattleEndConditionTests();
    assert(battleEndConditionFailures == 0);
    if (battleEndConditionFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Battle end-condition test suite\n";

    const int parserFailures = runParserTests();
    assert(parserFailures == 0);
    if (parserFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] ConfigParser test suite\n";

    // 运行游戏配置加载测试并把任何失败转换为非零退出码。
    const int gameConfigFailures = runGameConfigLoaderTests();
    assert(gameConfigFailures == 0);
    if (gameConfigFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] GameConfigLoader test suite\n";

    // 此代码段运行定义加载测试并在任一案例失败时终止程序。
    const int definitionFailures = runDefinitionConfigLoaderTests();
    assert(definitionFailures == 0);
    if (definitionFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] DefinitionConfigLoader test suite\n";

    // 此代码块运行第 5 天正式内容覆盖测试并传播任一失败。
    const int day5ContentFailures = runDay5ContentConfigTests();
    assert(day5ContentFailures == 0);
    if (day5ContentFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Day 5 content configuration test suite\n";

    // 此代码块运行单位继承和技能抽象接口测试并传播任一失败。
    const int unitSkillInterfaceFailures = runUnitSkillInterfaceTests();
    assert(unitSkillInterfaceFailures == 0);
    if (unitSkillInterfaceFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Unit and skill interface test suite\n";

    // 此代码块运行技力与技能目标规则测试并传播任一失败。
    const int skillSystemRuleFailures = runSkillSystemRuleTests();
    assert(skillSystemRuleFailures == 0);
    if (skillSystemRuleFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Skill system rule test suite\n";

    // 此代码块运行技能效果与持续时间测试并传播任一失败。
    const int skillEffectFailures = runSkillEffectTests();
    assert(skillEffectFailures == 0);
    if (skillEffectFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] Skill effect test suite\n";

    // 此代码段运行地图加载与路线校验测试并在任一案例失败时终止程序。
    const int mapFailures = runMapConfigLoaderTests();
    assert(mapFailures == 0);
    if (mapFailures != 0)
    {
        return 1;
    }

    std::cout << "[PASS] MapConfigLoader test suite\n";

    // 此代码段运行整包配置集成测试并把任何失败转换为非零退出码。
    const int configBundleFailures = runConfigBundleLoaderTests();
    assert(configBundleFailures == 0);
    if (configBundleFailures != 0)
    {
        return 1;
    }

    // 此输出标记整包加载器的全部集成案例已经通过。
    std::cout << "[PASS] ConfigBundleLoader test suite\n";
    return 0;
}
