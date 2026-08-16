#include "core/Core.hpp"
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
#include "core/economy/ShopService.hpp"
#include "core/map/MapTypes.hpp"
#include "core/model/Definitions.hpp"
#include "core/model/PlayerStateService.hpp"
#include "core/model/PlayerTypes.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <string>
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
            && formalBundle.units.size() == 1
            && formalBundle.factions.size() == 1)
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
            && skills.size() == 1
            && skills.front().id == "training_strike"
            && skills.front().effectType
                == autochess::core::SkillEffectType::Damage
            && skills.front().levelValues[2] == 45.0
            && units.size() == 1
            && units.front().id == "training_guard"
            && units.front().skillId == "training_strike"
            && units.front().tags.size() == 2
            && factions.size() == 1
            && factions.front().id == "training_team"
            && factions.front().maxDeployed == 4
            && modifiers.size() == 1
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
            && bundle.skills.size() == 1
            && bundle.units.size() == 1
            && bundle.factions.size() == 1
            && bundle.factionModifiers.size() == 1
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
