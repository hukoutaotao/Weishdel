#include "core/ai/AiController.hpp"

#include "core/ai/DefensiveStrategy.hpp"
#include "core/ai/OffensiveStrategy.hpp"
#include "core/ai/RouteStrategy.hpp"

#include <algorithm>

namespace autochess::core
{
    AiController::AiController(
        const ConfigBundle& config,
        const MapSide side,
        std::optional<std::string> forcedFactionId)
        : config_(config),
          side_(side),
          forcedFactionId_(std::move(forcedFactionId))
    {
        // 此构造函数只保存控制器依赖，不读取或修改任何对局状态。
    }

    MapSide AiController::side() const noexcept
    {
        // 此返回值保证应用层按固定阵营取得电脑快照。
        return side_;
    }

    std::optional<GameCommand> AiController::nextCommand(
        const ReadOnlyGameView& view)
    {
        // 此分支拒绝未知阵营控制器生成任何命令。
        if (side_ != MapSide::A && side_ != MapSide::B)
        {
            return std::nullopt;
        }

        // 此代码块在快照策略变化时重建对应的无状态策略对象。
        if (view.aiStrategy != selectedKind_)
        {
            selectStrategy(view.aiStrategy);
        }
        if (view.phase == MatchPhase::AiSelection
            && side_ == MapSide::B
            && view.aiStrategy != AiStrategyKind::Unknown
            && view.factionIdB.empty()
            && strategy_ != nullptr)
        {
            std::string factionId = forcedFactionId_.has_value()
                ? forcedFactionId_.value()
                : strategy_->defaultFactionId();
            // 此循环保证策略不会提交配置中不存在的分队 ID。
            const auto faction = std::find_if(
                view.factions.begin(),
                view.factions.end(),
                [&factionId](const SelectionOptionView& option)
                {
                    return option.id == factionId;
                });
            if (faction == view.factions.end())
            {
                return std::nullopt;
            }
            return GameCommand{
                side_,
                SelectFactionCommand{std::move(factionId)}};
        }

        // 此分支保持准备阶段以外没有额外的电脑命令。
        if (strategy_ == nullptr)
        {
            return std::nullopt;
        }
        if (view.phase == MatchPhase::Preparation)
        {
            return ownCommand(strategy_->choosePreparationCommand(
                side_,
                view,
                config_));
        }
        return std::nullopt;
    }

    void AiController::selectStrategy(const AiStrategyKind kind)
    {
        // 此代码块根据正式枚举创建唯一的具体策略对象。
        strategy_.reset();
        selectedKind_ = kind;
        if (kind == AiStrategyKind::Offensive)
        {
            strategy_ = std::make_unique<OffensiveStrategy>();
        }
        else if (kind == AiStrategyKind::Defensive)
        {
            strategy_ = std::make_unique<DefensiveStrategy>();
        }
        else if (kind == AiStrategyKind::Route)
        {
            strategy_ = std::make_unique<RouteStrategy>();
        }
    }

    std::optional<GameCommand> AiController::ownCommand(
        std::optional<GameCommand> command) const
    {
        // 此分支过滤策略错误返回的对手命令，避免绕过核心归属验证。
        if (command.has_value() && command->actor == side_)
        {
            return command;
        }
        return std::nullopt;
    }
}
