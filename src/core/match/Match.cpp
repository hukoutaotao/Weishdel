#include "core/match/Match.hpp"

#include "core/match/RoundController.hpp"
#include "core/model/PlayerStateService.hpp"

#include <utility>

// 此命名空间实现选择状态机和统一命令入口。
namespace autochess::core
{
    // 此匿名命名空间保存 Match 使用的统一结果构造函数。
    namespace
    {
        // 此辅助函数构造带错误码和中文消息的失败结果。
        CommandResult fail(
            const CommandErrorCode errorCode,
            const char* message)
        {
            return CommandResult{false, errorCode, message};
        }

        // 此辅助函数构造选择命令成功结果。
        CommandResult success(const char* message)
        {
            return CommandResult{
                true,
                CommandErrorCode::None,
                message};
        }
    }

    // 此构造函数保存配置并从配置种子创建唯一的对局随机引擎。
    Match::Match(ConfigBundle config)
        : config_(std::move(config)),
          randomEngine_(config_.gameConfig.randomSeed)
    {
    }

    // 此函数根据变体中实际保存的命令类型转发到对应处理函数。
    CommandResult Match::submit(const GameCommand& command)
    {
        // 此分支处理地图选择命令。
        if (const auto* payload =
                std::get_if<SelectMapCommand>(&command.payload))
        {
            return selectMap(command.actor, *payload);
        }

        // 此分支处理双方分队选择命令。
        if (const auto* payload =
                std::get_if<SelectFactionCommand>(&command.payload))
        {
            return selectFaction(command.actor, *payload);
        }

        // 此分支处理玩家选择电脑策略的命令。
        if (const auto* payload =
                std::get_if<SelectAiStrategyCommand>(&command.payload))
        {
            return selectAiStrategy(command.actor, *payload);
        }

        return fail(
            CommandErrorCode::InvalidPhase,
            "当前步骤尚不接受该类型命令");
    }

    // 此函数验证玩家只能在初始阶段选择存在的地图。
    CommandResult Match::selectMap(
        const MapSide actor,
        const SelectMapCommand& command)
    {
        // 此分支拒绝地图选择阶段以外的重复或越序操作。
        if (phase_ != MatchPhase::MapSelection)
        {
            return fail(
                CommandErrorCode::InvalidPhase,
                "当前阶段不能选择地图");
        }

        // 此分支只允许 A 方玩家选择本局地图。
        if (actor != MapSide::A)
        {
            return fail(
                CommandErrorCode::InvalidActor,
                "只有A方玩家可以选择地图");
        }

        // 此分支拒绝配置中不存在的地图 ID。
        if (findMap(command.mapId) == nullptr)
        {
            return fail(
                CommandErrorCode::InvalidSelection,
                "选择的地图不存在");
        }

        selectedMapId_ = command.mapId;
        phase_ = MatchPhase::FactionSelection;
        return success("地图选择成功");
    }

    // 此函数先处理 A 方选择，再在 AI 策略确定后处理 B 方选择。
    CommandResult Match::selectFaction(
        const MapSide actor,
        const SelectFactionCommand& command)
    {
        // 此分支处理玩家自己的分队选择并推进到 AI 选择阶段。
        if (phase_ == MatchPhase::FactionSelection)
        {
            // 此分支只允许 A 方选择玩家分队。
            if (actor != MapSide::A)
            {
                return fail(
                    CommandErrorCode::InvalidActor,
                    "只有A方玩家可以选择玩家分队");
            }

            // 此分支拒绝配置中不存在的玩家分队。
            if (findFaction(command.factionId) == nullptr)
            {
                return fail(
                    CommandErrorCode::InvalidSelection,
                    "选择的玩家分队不存在");
            }

            factionIdA_ = command.factionId;
            phase_ = MatchPhase::AiSelection;
            return success("玩家分队选择成功");
        }

        // 此分支拒绝 AI 策略尚未确定时提前选择 B 方分队。
        if (phase_ != MatchPhase::AiSelection
            || aiStrategy_ == AiStrategyKind::Unknown)
        {
            return fail(
                CommandErrorCode::InvalidPhase,
                "当前阶段不能选择电脑分队");
        }

        // 此分支只允许 B 方控制器提交电脑分队选择。
        if (actor != MapSide::B)
        {
            return fail(
                CommandErrorCode::InvalidActor,
                "只有B方控制器可以选择电脑分队");
        }

        const FactionDefinition* factionA = findFaction(factionIdA_);
        const FactionDefinition* factionB = findFaction(command.factionId);
        // 此分支拒绝配置中不存在的双方分队定义。
        if (factionA == nullptr || factionB == nullptr)
        {
            return fail(
                CommandErrorCode::InvalidSelection,
                "选择的电脑分队不存在");
        }

        PlayerState updatedPlayerA = PlayerStateService::createInitial(
            MapSide::A,
            config_.gameConfig,
            *factionA);
        PlayerState updatedPlayerB = PlayerStateService::createInitial(
            MapSide::B,
            config_.gameConfig,
            *factionB);
        ShopState updatedShopA;
        ShopState updatedShopB;
        std::mt19937 updatedRandomEngine = randomEngine_;
        const CommandResult preparation =
            RoundController::beginPreparation(
                updatedPlayerA,
                updatedPlayerB,
                updatedShopA,
                updatedShopB,
                config_.gameConfig,
                config_.units,
                config_.factions,
                config_.factionModifiers,
                MapSide::Unknown,
                updatedRandomEngine);
        // 此分支在首回合初始化失败时保留 AI 选择阶段的原状态。
        if (!preparation.success)
        {
            return preparation;
        }

        factionIdB_ = command.factionId;
        playerA_ = std::move(updatedPlayerA);
        playerB_ = std::move(updatedPlayerB);
        shopA_ = std::move(updatedShopA);
        shopB_ = std::move(updatedShopB);
        randomEngine_ = std::move(updatedRandomEngine);
        playersCreated_ = true;
        currentRound_ = 1;
        phase_ = MatchPhase::Preparation;
        return success("电脑分队选择成功，第一回合准备开始");
    }

    // 此函数只允许 A 方在 AI 选择阶段提交一个有效策略。
    CommandResult Match::selectAiStrategy(
        const MapSide actor,
        const SelectAiStrategyCommand& command)
    {
        // 此分支拒绝 AI 选择阶段以外的策略命令。
        if (phase_ != MatchPhase::AiSelection)
        {
            return fail(
                CommandErrorCode::InvalidPhase,
                "当前阶段不能选择电脑策略");
        }

        // 此分支只允许 A 方玩家选择电脑策略。
        if (actor != MapSide::A)
        {
            return fail(
                CommandErrorCode::InvalidActor,
                "只有A方玩家可以选择电脑策略");
        }

        // 此分支拒绝未知策略以及同一阶段的重复选择。
        if (command.strategy == AiStrategyKind::Unknown
            || aiStrategy_ != AiStrategyKind::Unknown)
        {
            return fail(
                CommandErrorCode::InvalidSelection,
                "电脑策略无效或已经选择");
        }

        aiStrategy_ = command.strategy;
        return success("电脑策略选择成功");
    }

    // 此函数线性查找数量很少且顺序固定的地图配置。
    const MapDefinition* Match::findMap(
        const std::string& mapId) const noexcept
    {
        // 此循环在正式地图列表中查找目标 ID。
        for (const MapDefinition& map : config_.maps)
        {
            // 此分支在找到目标地图时返回其稳定只读地址。
            if (map.id == mapId)
            {
                return &map;
            }
        }

        return nullptr;
    }

    // 此函数线性查找数量很少且顺序固定的分队配置。
    const FactionDefinition* Match::findFaction(
        const std::string& factionId) const noexcept
    {
        // 此循环在正式分队列表中查找目标 ID。
        for (const FactionDefinition& faction : config_.factions)
        {
            // 此分支在找到目标分队时返回其稳定只读地址。
            if (faction.id == factionId)
            {
                return &faction;
            }
        }

        return nullptr;
    }

    // 此函数返回当前状态机阶段。
    MatchPhase Match::phase() const noexcept
    {
        return phase_;
    }

    // 此函数返回已经开始的当前回合号。
    int Match::currentRound() const noexcept
    {
        return currentRound_;
    }

    // 此函数返回当前选择的地图 ID。
    const std::string& Match::selectedMapId() const noexcept
    {
        return selectedMapId_;
    }

    // 此函数返回当前选择的 AI 策略类型。
    AiStrategyKind Match::selectedAiStrategy() const noexcept
    {
        return aiStrategy_;
    }

    // 此函数在玩家已经创建后按阵营返回内部状态的只读地址。
    const PlayerState* Match::playerState(const MapSide side) const noexcept
    {
        // 此分支在选择阶段隐藏尚未创建的玩家状态。
        if (!playersCreated_)
        {
            return nullptr;
        }

        // 此分支把有效阵营映射到对应的玩家状态。
        if (side == MapSide::A)
        {
            return &playerA_;
        }
        if (side == MapSide::B)
        {
            return &playerB_;
        }

        return nullptr;
    }

    // 此函数在玩家已经创建后按阵营返回内部商店的只读地址。
    const ShopState* Match::shopState(const MapSide side) const noexcept
    {
        // 此分支在选择阶段隐藏尚未创建的商店状态。
        if (!playersCreated_)
        {
            return nullptr;
        }

        // 此分支把有效阵营映射到对应的商店状态。
        if (side == MapSide::A)
        {
            return &shopA_;
        }
        if (side == MapSide::B)
        {
            return &shopB_;
        }

        return nullptr;
    }
}
