#pragma once

#include "core/combat/BattleTypes.hpp"
#include "core/economy/EconomyTypes.hpp"
#include "core/map/MapTypes.hpp"
#include "core/match/MatchTypes.hpp"
#include "core/model/PlayerTypes.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// 此命名空间定义界面和控制器可安全复制的对局快照。
namespace autochess::core
{
    // 此结构保存选择界面需要显示的稳定 ID 和中文名称。
    struct SelectionOptionView
    {
        std::string id;
        std::string name;
    };

    // 此结构保存对手一个公开部署单位的身份和格子位置。
    struct PublicDeployedUnitView
    {
        OwnedUnitId id = InvalidOwnedUnitId;
        UnitIdentity identity;
        GridPosition position;
    };

    // 此结构只公开对手守卫、分队和当前部署信息。
    struct PublicOpponentView
    {
        bool available = false;
        MapSide side = MapSide::Unknown;
        std::string factionId;
        int guardValue = 0;
        std::vector<PublicDeployedUnitView> deployments;
    };

    // 此结构是按观察阵营生成且不引用 Match 内部对象的完整快照。
    struct ReadOnlyGameView
    {
        MapSide viewer = MapSide::Unknown;
        MatchPhase phase = MatchPhase::MapSelection;
        int currentRound = 0;
        int maxRounds = 0;
        std::uint64_t preparationFramesRemaining = 0;
        std::vector<SelectionOptionView> maps;
        std::vector<SelectionOptionView> factions;
        std::vector<AiStrategyKind> aiStrategies;
        std::string selectedMapId;
        std::string factionIdA;
        std::string factionIdB;
        AiStrategyKind aiStrategy = AiStrategyKind::Unknown;
        std::optional<MapDefinition> selectedMap;
        std::optional<PlayerState> self;
        std::optional<ShopState> selfShop;
        PublicOpponentView opponent;
        std::vector<BattleUnit> battleUnits;
        std::optional<RoundSummary> lastRound;
        MatchResultSummary result;
    };
}
