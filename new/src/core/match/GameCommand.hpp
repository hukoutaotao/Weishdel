#pragma once

#include "core/map/MapTypes.hpp"
#include "core/match/MatchTypes.hpp"
#include "core/model/PlayerTypes.hpp"

#include <cstddef>
#include <string>
#include <variant>

// 此命名空间定义真人、脚本和未来 AI 共用的命令协议。
namespace autochess::core
{
    // 此命令请求选择本局使用的地图。
    struct SelectMapCommand
    {
        std::string mapId;
    };

    // 此命令请求为执行方选择一个分队。
    struct SelectFactionCommand
    {
        std::string factionId;
    };

    // 此命令请求选择电脑使用的策略类型。
    struct SelectAiStrategyCommand
    {
        AiStrategyKind strategy = AiStrategyKind::Unknown;
    };

    // 此命令请求付费刷新执行方商店。
    struct RefreshShopCommand
    {
    };

    // 此命令请求购买指定商店槽位的单位。
    struct PurchaseUnitCommand
    {
        std::size_t shopSlot = 0;
    };

    // 此命令请求把持久单位移动到合法部署格。
    struct MoveToDeploymentCommand
    {
        OwnedUnitId unitId = InvalidOwnedUnitId;
        GridPosition target;
    };

    // 此命令请求把持久单位移动到指定备用槽。
    struct MoveToReserveCommand
    {
        OwnedUnitId unitId = InvalidOwnedUnitId;
        std::size_t reserveSlot = 0;
    };

    // 此命令请求把两个同类型同等级单位合成为高一级单位。
    struct MergeUnitsCommand
    {
        OwnedUnitId sourceUnitId = InvalidOwnedUnitId;
        OwnedUnitId targetUnitId = InvalidOwnedUnitId;
    };

    // 此命令请求出售指定活动单位。
    struct SellUnitCommand
    {
        OwnedUnitId unitId = InvalidOwnedUnitId;
    };

    // 此命令请求复活指定死亡单位。
    struct ReviveUnitCommand
    {
        OwnedUnitId unitId = InvalidOwnedUnitId;
    };

    // 此命令请求由玩家提前结束准备并开始战斗。
    struct StartCombatCommand
    {
    };

    // 此变体限定统一入口能够接收的全部命令类型。
    using GameCommandPayload = std::variant<
        SelectMapCommand,
        SelectFactionCommand,
        SelectAiStrategyCommand,
        RefreshShopCommand,
        PurchaseUnitCommand,
        MoveToDeploymentCommand,
        MoveToReserveCommand,
        MergeUnitsCommand,
        SellUnitCommand,
        ReviveUnitCommand,
        StartCombatCommand>;

    // 此结构把执行阵营和具体命令负载封装成统一输入。
    struct GameCommand
    {
        MapSide actor = MapSide::Unknown;
        GameCommandPayload payload = RefreshShopCommand{};
    };
}
