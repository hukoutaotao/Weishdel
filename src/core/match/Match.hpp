#pragma once

#include "core/config/ConfigBundleLoader.hpp"
#include "core/economy/EconomyTypes.hpp"
#include "core/match/GameCommand.hpp"
#include "core/match/ReadOnlyGameView.hpp"

#include <random>
#include <string>

// 此命名空间声明拥有完整对局状态的核心编排对象。
namespace autochess::core
{
    // 此类验证统一命令并协调选择、准备、战斗和结算服务。
    class Match
    {
    public:
        // 此构造函数复制已校验配置并使用正式随机种子初始化对局。
        explicit Match(ConfigBundle config);

        // 此函数验证并执行一条来自真人、脚本或 AI 的统一命令。
        CommandResult submit(const GameCommand& command);

        // 此函数返回当前对局阶段供界面和测试判断可用操作。
        MatchPhase phase() const noexcept;

        // 此函数返回当前回合号且选择阶段返回零。
        int currentRound() const noexcept;

        // 此函数返回当前选择的地图 ID。
        const std::string& selectedMapId() const noexcept;

        // 此函数返回当前选择的 AI 策略。
        AiStrategyKind selectedAiStrategy() const noexcept;

        // 此函数按阵营返回只读玩家状态且未创建时返回空指针。
        const PlayerState* playerState(MapSide side) const noexcept;

        // 此函数按阵营返回只读商店状态且未创建时返回空指针。
        const ShopState* shopState(MapSide side) const noexcept;

        // 此函数生成按观察阵营裁剪且不引用内部对象的值拷贝快照。
        ReadOnlyGameView viewFor(MapSide viewer) const;

    private:
        // 此函数执行地图选择并推进到玩家分队选择阶段。
        CommandResult selectMap(
            MapSide actor,
            const SelectMapCommand& command);

        // 此函数根据当前选择阶段执行 A 方或 B 方分队选择。
        CommandResult selectFaction(
            MapSide actor,
            const SelectFactionCommand& command);

        // 此函数记录玩家选择的电脑策略并等待 B 方分队命令。
        CommandResult selectAiStrategy(
            MapSide actor,
            const SelectAiStrategyCommand& command);

        // 此函数把准备阶段经济命令转发到现有规则服务。
        CommandResult executePreparationCommand(const GameCommand& command);

        // 此函数按阵营返回可修改的玩家状态。
        PlayerState* mutablePlayer(MapSide side) noexcept;

        // 此函数按阵营返回可修改的商店状态。
        ShopState* mutableShop(MapSide side) noexcept;

        // 此函数判断指定 ID 是否存在于玩家活动或死亡名单中。
        static bool containsOwnedUnit(
            const PlayerState& player,
            OwnedUnitId unitId) noexcept;

        // 此函数查找配置中的地图定义。
        const MapDefinition* findMap(const std::string& mapId) const noexcept;

        // 此函数查找配置中的分队定义。
        const FactionDefinition* findFaction(
            const std::string& factionId) const noexcept;

        ConfigBundle config_;
        std::mt19937 randomEngine_;
        MatchPhase phase_ = MatchPhase::MapSelection;
        int currentRound_ = 0;
        std::string selectedMapId_;
        std::string factionIdA_;
        std::string factionIdB_;
        AiStrategyKind aiStrategy_ = AiStrategyKind::Unknown;
        PlayerState playerA_;
        PlayerState playerB_;
        ShopState shopA_;
        ShopState shopB_;
        bool playersCreated_ = false;
        OwnedUnitId nextOwnedUnitId_ = 1;
        std::uint64_t preparationFramesRemaining_ = 0;
        std::optional<RoundSummary> lastRound_;
        MatchResultSummary result_;
    };
}
