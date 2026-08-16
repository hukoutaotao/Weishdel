#include "core/Core.hpp"
#include "core/config/ConfigBundleLoader.hpp"
#include "core/config/ConfigError.hpp"
#include "core/config/DefinitionConfigLoader.hpp"
#include "core/config/GameConfigLoader.hpp"
#include "core/config/MapConfigLoader.hpp"
#include "core/config/ConfigParser.hpp"
#include "core/economy/EconomyTypes.hpp"
#include "core/map/MapTypes.hpp"
#include "core/model/Definitions.hpp"
#include "core/model/PlayerTypes.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
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
