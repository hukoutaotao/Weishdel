#pragma once

#include "core/ai/IAiStrategy.hpp"
#include "core/controllers/IPlayerController.hpp"

#include <memory>
#include <optional>
#include <string>

// 此命名空间声明正式电脑控制器，不依赖 SFML 表现层。
namespace autochess::core
{
    // 此类把策略选择、分队选择和统一命令生成连接起来。
    class AiController final : public IPlayerController
    {
    public:
        // 此构造函数保存只读配置、控制阵营和可选测试分队覆盖。
        AiController(
            const ConfigBundle& config,
            MapSide side = MapSide::B,
            std::optional<std::string> forcedFactionId = std::nullopt);

        // 此函数返回电脑控制器负责的阵营。
        MapSide side() const noexcept override;

        // 此函数根据最新快照生成至多一条合法统一命令。
        std::optional<GameCommand> nextCommand(
            const ReadOnlyGameView& view) override;

    private:
        // 此函数按界面选择创建对应的无状态策略对象。
        void selectStrategy(AiStrategyKind kind);

        // 此函数检查策略生成的命令确实属于当前控制器。
        std::optional<GameCommand> ownCommand(
            std::optional<GameCommand> command) const;

        const ConfigBundle& config_;
        MapSide side_ = MapSide::Unknown;
        std::optional<std::string> forcedFactionId_;
        AiStrategyKind selectedKind_ = AiStrategyKind::Unknown;
        std::unique_ptr<IAiStrategy> strategy_;
    };
}
