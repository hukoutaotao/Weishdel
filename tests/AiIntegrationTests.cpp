#include "AiIntegrationTests.hpp"

#include "core/ai/AiController.hpp"
#include "core/config/ConfigBundleLoader.hpp"
#include "core/match/Match.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

namespace
{
    using namespace autochess::core;

    constexpr std::uint64_t MaxScenarioFrames = 1000;

    // 此运行器逐项输出三十六组矩阵结果并累计失败数量。
    class AiIntegrationRunner
    {
    public:
        // 此函数记录一个矩阵场景是否满足全部结束条件。
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

        // 此函数返回累计失败场景数。
        int failureCount() const noexcept
        {
            return failureCount_;
        }

    private:
        int failureCount_ = 0;
    };

    // 此结构保存一个完整自动对局场景的可验收数据。
    struct AiScenarioResult
    {
        bool coreStepsSucceeded = true;
        bool reachedResult = false;
        bool selectionsPreserved = false;
        std::size_t aiCommandCount = 0;
        std::size_t commandFailureCount = 0;
        std::uint64_t elapsedFrames = 0;
        MatchResultSummary result;
    };

    // 此函数把策略枚举转换成稳定的测试输出文本。
    const char* strategyName(const AiStrategyKind strategy) noexcept
    {
        // 此代码块覆盖三种正式策略并为异常值保留明确名称。
        if (strategy == AiStrategyKind::Offensive)
        {
            return "offensive";
        }
        if (strategy == AiStrategyKind::Defensive)
        {
            return "defensive";
        }
        if (strategy == AiStrategyKind::Route)
        {
            return "route";
        }
        return "unknown";
    }

    // 此函数把最终胜负转换成稳定的测试输出文本。
    const char* outcomeName(const MatchOutcome outcome) noexcept
    {
        // 此代码块覆盖最终结果和未结束状态，便于定位帧保护失败。
        if (outcome == MatchOutcome::SideAWin)
        {
            return "A_win";
        }
        if (outcome == MatchOutcome::SideBWin)
        {
            return "B_win";
        }
        if (outcome == MatchOutcome::Draw)
        {
            return "draw";
        }
        return "ongoing";
    }

    // 此函数提交一条选择命令并把拒绝计入场景失败数。
    bool submitSelection(
        Match& match,
        const GameCommand& command,
        AiScenarioResult& result)
    {
        const CommandResult commandResult = match.submit(command);
        // 此分支记录正式命令入口拒绝的任何选择命令。
        if (!commandResult.success)
        {
            ++result.commandFailureCount;
            return false;
        }
        return true;
    }

    // 此函数运行一个真实 Match 与正式 AI 控制器组成的三回合场景。
    AiScenarioResult runScenario(
        const ConfigBundle& sourceConfig,
        const AiStrategyKind strategy,
        const std::string& mapId,
        const std::string& forcedFactionId)
    {
        ConfigBundle config = sourceConfig;
        // 此代码块只缩短测试副本时间，不修改磁盘上的正式配置。
        config.gameConfig.preparationSeconds = 1;
        config.gameConfig.combatTimeoutSeconds = 1;

        Match match(config);
        AiController controller(config, MapSide::B, forcedFactionId);
        AiScenarioResult result;

        // 此代码块按正式依赖顺序完成 A 方地图、分队和策略选择。
        const bool mapSelected = submitSelection(
            match,
            {MapSide::A, SelectMapCommand{mapId}},
            result);
        const bool factionSelected = mapSelected && submitSelection(
            match,
            {MapSide::A, SelectFactionCommand{"training_team"}},
            result);
        const bool strategySelected = factionSelected && submitSelection(
            match,
            {MapSide::A, SelectAiStrategyCommand{strategy}},
            result);
        // 此分支在基础选择失败时保留诊断结果并停止推进。
        if (!strategySelected)
        {
            return result;
        }

        // 此循环让 AI 每帧最多提交一条命令，再推进一个核心固定帧。
        while (result.elapsedFrames < MaxScenarioFrames
               && match.phase() != MatchPhase::MatchResult)
        {
            const ReadOnlyGameView view = match.viewFor(MapSide::B);
            const std::optional<GameCommand> command =
                controller.nextCommand(view);
            // 此代码块把 AI 的每条命令都交给真实 Match 权限与阶段校验。
            if (command.has_value())
            {
                ++result.aiCommandCount;
                const CommandResult commandResult = match.submit(
                    command.value());
                if (!commandResult.success)
                {
                    ++result.commandFailureCount;
                }
            }

            const MatchPhase phase = match.phase();
            // 此代码块只推进核心定义为可步进的三个阶段。
            if (phase == MatchPhase::Preparation
                || phase == MatchPhase::Combat
                || phase == MatchPhase::RoundSettlement)
            {
                if (!match.step())
                {
                    result.coreStepsSucceeded = false;
                    break;
                }
            }
            ++result.elapsedFrames;
        }

        const ReadOnlyGameView finalView = match.viewFor(MapSide::B);
        result.reachedResult = finalView.phase == MatchPhase::MatchResult;
        result.selectionsPreserved =
            finalView.selectedMapId == mapId
            && finalView.factionIdA == "training_team"
            && finalView.factionIdB == forcedFactionId
            && finalView.aiStrategy == strategy;
        result.result = finalView.result;
        return result;
    }

    // 此函数输出一个场景的命令、帧数和最终胜负诊断。
    void printScenarioSummary(
        const AiStrategyKind strategy,
        const std::string& mapId,
        const std::string& factionId,
        const AiScenarioResult& result)
    {
        std::cout
            << "[AI MATRIX] strategy=" << strategyName(strategy)
            << " map=" << mapId
            << " faction=" << factionId
            << " frames=" << result.elapsedFrames
            << " ai_commands=" << result.aiCommandCount
            << " command_failures=" << result.commandFailureCount
            << " outcome=" << outcomeName(result.result.outcome)
            << " rounds=" << result.result.completedRounds
            << '\n';
    }
}

// 此函数运行并验收全部三十六个 AI 对局组合。
int runAiIntegrationTests()
{
    AiIntegrationRunner runner;
    ConfigBundle config;
    ConfigError error;
    const bool loaded = ConfigBundleLoader::load(
        AUTOCHESS_DATA_DIR,
        config,
        error);
    runner.check(loaded, "AI integration matrix loads formal configuration");
    // 此分支在正式配置加载失败时停止构造无效场景。
    if (!loaded)
    {
        return runner.failureCount();
    }

    const std::array<AiStrategyKind, 3> strategies = {
        AiStrategyKind::Offensive,
        AiStrategyKind::Defensive,
        AiStrategyKind::Route};
    const std::array<std::string, 4> maps = {
        "map_01", "map_02", "map_03", "map_04"};
    const std::array<std::string, 3> factions = {
        "training_team",
        "assault_team",
        "route_team"};

    // 此代码块按策略、地图、分队顺序稳定运行三乘四乘三矩阵。
    std::size_t scenarioCount = 0;
    for (const AiStrategyKind strategy : strategies)
    {
        for (const std::string& mapId : maps)
        {
            for (const std::string& factionId : factions)
            {
                const AiScenarioResult result = runScenario(
                    config,
                    strategy,
                    mapId,
                    factionId);
                printScenarioSummary(strategy, mapId, factionId, result);

                const bool passed = result.coreStepsSucceeded
                    && result.reachedResult
                    && result.selectionsPreserved
                    && result.aiCommandCount > 0
                    && result.commandFailureCount == 0
                    && result.elapsedFrames < MaxScenarioFrames
                    && result.result.outcome != MatchOutcome::Ongoing
                    && result.result.completedRounds
                        == config.gameConfig.maxRounds;
                runner.check(
                    passed,
                    std::string("AI integration ")
                        + strategyName(strategy)
                        + ' ' + mapId + ' ' + factionId);
                ++scenarioCount;
            }
        }
    }

    // 此断言保证循环边界没有遗漏或重复之外的组合数量。
    runner.check(
        scenarioCount == 36,
        "AI integration matrix executes exactly thirty-six scenarios");
    return runner.failureCount();
}
