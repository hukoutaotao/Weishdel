#include "core/match/Match.hpp"

#include "core/match/RoundController.hpp"
#include "core/economy/DeploymentService.hpp"
#include "core/economy/MergeService.hpp"
#include "core/economy/RosterService.hpp"
#include "core/economy/ShopService.hpp"
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

        return executePreparationCommand(command);
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

    // 此函数只在准备阶段把命令转交给经过验证的经济和部署服务。
    CommandResult Match::executePreparationCommand(
        const GameCommand& command)
    {
        // 此分支拒绝准备阶段以外的所有经济和部署命令。
        if (phase_ != MatchPhase::Preparation)
        {
            return fail(
                CommandErrorCode::InvalidPhase,
                "当前阶段不能执行准备操作");
        }

        PlayerState* player = mutablePlayer(command.actor);
        ShopState* shop = mutableShop(command.actor);
        // 此分支拒绝未知阵营或尚未创建玩家的命令。
        if (player == nullptr || shop == nullptr)
        {
            return fail(
                CommandErrorCode::InvalidActor,
                "准备命令的执行方无效");
        }

        PlayerState* opponent = command.actor == MapSide::A
            ? mutablePlayer(MapSide::B)
            : mutablePlayer(MapSide::A);
        const FactionDefinition* faction = findFaction(player->factionId);
        const MapDefinition* map = findMap(selectedMapId_);
        // 此分支拒绝对局内部引用已经缺失的地图或分队定义。
        if (faction == nullptr || map == nullptr || opponent == nullptr)
        {
            return fail(
                CommandErrorCode::InvalidConfiguration,
                "对局引用的地图或分队定义不存在");
        }

        // 此分支执行付费刷新并让服务原子管理随机引擎。
        if (std::holds_alternative<RefreshShopCommand>(command.payload))
        {
            return ShopService::refresh(
                *player,
                *shop,
                config_.gameConfig,
                config_.units,
                *faction,
                config_.factionModifiers,
                randomEngine_);
        }

        // 此分支执行指定商店槽位的购买命令。
        if (const auto* payload =
                std::get_if<PurchaseUnitCommand>(&command.payload))
        {
            return ShopService::purchase(
                *player,
                *shop,
                payload->shopSlot,
                config_.units,
                nextOwnedUnitId_);
        }

        // 此局部函数拒绝操作明确属于对手的持久单位。
        const auto rejectOpponentUnit =
            [opponent](const OwnedUnitId unitId) -> CommandResult
        {
            // 此分支把跨阵营单位操作报告为明确的归属错误。
            if (containsOwnedUnit(*opponent, unitId))
            {
                return fail(
                    CommandErrorCode::WrongOwner,
                    "不能操作对手持有的单位");
            }

            return CommandResult{
                true,
                CommandErrorCode::None,
                "单位归属检查通过"};
        };

        // 此分支执行从备用区或其他部署格移动到部署区的命令。
        if (const auto* payload =
                std::get_if<MoveToDeploymentCommand>(&command.payload))
        {
            const CommandResult ownership =
                rejectOpponentUnit(payload->unitId);
            // 此分支在发现对手单位时阻止部署服务继续执行。
            if (!ownership.success)
            {
                return ownership;
            }

            return DeploymentService::moveToDeployment(
                *player,
                *map,
                *faction,
                payload->unitId,
                payload->target);
        }

        // 此分支执行单位返回指定备用槽的命令。
        if (const auto* payload =
                std::get_if<MoveToReserveCommand>(&command.payload))
        {
            const CommandResult ownership =
                rejectOpponentUnit(payload->unitId);
            // 此分支在发现对手单位时阻止备用区移动继续执行。
            if (!ownership.success)
            {
                return ownership;
            }

            return DeploymentService::moveToReserve(
                *player,
                payload->unitId,
                payload->reserveSlot);
        }

        // 此分支执行两个持久单位的同类同级合成命令。
        if (const auto* payload =
                std::get_if<MergeUnitsCommand>(&command.payload))
        {
            const CommandResult sourceOwnership =
                rejectOpponentUnit(payload->sourceUnitId);
            const CommandResult targetOwnership =
                rejectOpponentUnit(payload->targetUnitId);
            // 此分支在任一单位属于对手时拒绝整个合成命令。
            if (!sourceOwnership.success)
            {
                return sourceOwnership;
            }
            if (!targetOwnership.success)
            {
                return targetOwnership;
            }

            return MergeService::merge(
                *player,
                payload->sourceUnitId,
                payload->targetUnitId,
                config_.gameConfig,
                config_.units,
                *faction,
                config_.factionModifiers,
                nextOwnedUnitId_);
        }

        // 此分支执行出售活动单位并返还配置比例金币的命令。
        if (const auto* payload =
                std::get_if<SellUnitCommand>(&command.payload))
        {
            const CommandResult ownership =
                rejectOpponentUnit(payload->unitId);
            // 此分支在发现对手单位时阻止出售继续执行。
            if (!ownership.success)
            {
                return ownership;
            }

            return RosterService::sell(
                *player,
                payload->unitId,
                config_.gameConfig,
                config_.units,
                *faction,
                config_.factionModifiers);
        }

        // 此分支执行死亡单位复活并放入首个空备用槽的命令。
        if (const auto* payload =
                std::get_if<ReviveUnitCommand>(&command.payload))
        {
            const CommandResult ownership =
                rejectOpponentUnit(payload->unitId);
            // 此分支在发现对手单位时阻止复活继续执行。
            if (!ownership.success)
            {
                return ownership;
            }

            return RosterService::revive(
                *player,
                payload->unitId,
                config_.gameConfig,
                config_.units,
                *faction,
                config_.factionModifiers);
        }

        return fail(
            CommandErrorCode::InvalidPhase,
            "当前准备阶段尚不接受该类型命令");
    }

    // 此函数按阵营返回可修改的玩家状态供内部命令处理使用。
    PlayerState* Match::mutablePlayer(const MapSide side) noexcept
    {
        // 此分支在双方玩家尚未创建时拒绝访问状态。
        if (!playersCreated_)
        {
            return nullptr;
        }

        // 此分支把有效阵营映射到对应玩家状态。
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

    // 此函数按阵营返回可修改的商店状态供内部命令处理使用。
    ShopState* Match::mutableShop(const MapSide side) noexcept
    {
        // 此分支在双方玩家尚未创建时拒绝访问商店。
        if (!playersCreated_)
        {
            return nullptr;
        }

        // 此分支把有效阵营映射到对应商店状态。
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

    // 此函数在活动和死亡列表中查找指定持久单位 ID。
    bool Match::containsOwnedUnit(
        const PlayerState& player,
        const OwnedUnitId unitId) noexcept
    {
        return PlayerStateService::findActive(player, unitId) != nullptr
            || PlayerStateService::findDead(player, unitId) != nullptr;
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

    // 此函数构造完整值拷贝并按观察阵营隐藏对手私有经济信息。
    ReadOnlyGameView Match::viewFor(const MapSide viewer) const
    {
        ReadOnlyGameView view;
        view.viewer = viewer;
        view.phase = phase_;
        view.currentRound = currentRound_;
        view.maxRounds = config_.gameConfig.maxRounds;
        view.preparationFramesRemaining = preparationFramesRemaining_;
        view.selectedMapId = selectedMapId_;
        view.factionIdA = factionIdA_;
        view.factionIdB = factionIdB_;
        view.aiStrategy = aiStrategy_;
        view.lastRound = lastRound_;
        view.result = result_;
        view.aiStrategies = {
            AiStrategyKind::Offensive,
            AiStrategyKind::Defensive,
            AiStrategyKind::Route};

        // 此循环复制地图选择所需的 ID 和中文名称。
        for (const MapDefinition& map : config_.maps)
        {
            view.maps.push_back(SelectionOptionView{map.id, map.name});
        }

        // 此循环复制分队选择所需的 ID 和中文名称。
        for (const FactionDefinition& faction : config_.factions)
        {
            view.factions.push_back(
                SelectionOptionView{faction.id, faction.name});
        }

        const MapDefinition* selectedMap = findMap(selectedMapId_);
        // 此分支只在地图已经选择后复制完整地图定义。
        if (selectedMap != nullptr)
        {
            view.selectedMap = *selectedMap;
        }

        const PlayerState* self = playerState(viewer);
        const ShopState* selfShop = shopState(viewer);
        const MapSide opponentSide = viewer == MapSide::A
            ? MapSide::B
            : (viewer == MapSide::B ? MapSide::A : MapSide::Unknown);
        const PlayerState* opponent = playerState(opponentSide);

        // 此分支向观察者复制自己的完整持久状态和商店。
        if (self != nullptr && selfShop != nullptr)
        {
            view.self = *self;
            view.selfShop = *selfShop;
        }

        // 此分支只向观察者复制对手的公开守卫、分队和部署单位。
        if (opponent != nullptr)
        {
            view.opponent.available = true;
            view.opponent.side = opponent->side;
            view.opponent.factionId = opponent->factionId;
            view.opponent.guardValue = opponent->guardValue;
            // 此循环把对手部署映射转换为不含私有状态的公开单位列表。
            for (const auto& deployment : opponent->deployments)
            {
                const OwnedUnit* unit = PlayerStateService::findActive(
                    *opponent,
                    deployment.second);
                // 此分支只复制仍存在于活动名单中的有效部署单位。
                if (unit != nullptr)
                {
                    view.opponent.deployments.push_back(
                        PublicDeployedUnitView{
                            unit->id,
                            unit->identity,
                            deployment.first});
                }
            }
        }

        return view;
    }
}
