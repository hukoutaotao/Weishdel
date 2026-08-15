#include "core/Core.hpp"
#include "core/config/ConfigError.hpp"
#include "core/config/GameConfigLoader.hpp"
#include "core/config/ConfigParser.hpp"
#include "core/map/MapTypes.hpp"
#include "core/model/Definitions.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
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
            && error.category == expectedCategory
            && error.sourcePath == path
            && error.line == expectedLine
            && !error.message.empty()
            && !autochess::core::formatConfigError(error).empty();
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
            && error.category == expectedCategory
            && error.sourcePath == path
            && error.line == expectedLine
            && !error.message.empty()
            && !autochess::core::formatConfigError(error).empty();
    }

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
}

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
    return 0;
}
