#include "core/Core.hpp"
#include "core/config/ConfigError.hpp"
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
    return 0;
}
