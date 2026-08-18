#include "game/controllers/BootstrapComputerController.hpp"

#include <set>

namespace autochess::game
{
    // 此函数固定占位电脑控制 B 方。
    core::MapSide BootstrapComputerController::side() const noexcept
    {
        return core::MapSide::B;
    }

    // 此函数按选择、部署、购买的依赖顺序生成至多一条合法命令。
    std::optional<core::GameCommand>
    BootstrapComputerController::nextCommand(
        const core::ReadOnlyGameView& view)
    {
        // 此代码块在策略已经选择后为 B 方提交固定映射分队。
        if (view.phase == core::MatchPhase::AiSelection
            && view.aiStrategy != core::AiStrategyKind::Unknown
            && view.factionIdB.empty())
        {
            return core::GameCommand{
                core::MapSide::B,
                core::SelectFactionCommand{factionFor(view.aiStrategy)}};
        }

        // 此代码块在非准备阶段或缺失自身数据时保持占位电脑静默。
        if (view.phase != core::MatchPhase::Preparation
            || !view.self.has_value()
            || !view.selfShop.has_value()
            || !view.selectedMap.has_value())
        {
            return std::nullopt;
        }

        const core::PlayerState& player = view.self.value();
        const core::MapDefinition& map = view.selectedMap.value();
        std::set<core::GridPosition> occupiedStarts;
        // 此代码块记录已经部署的格子以便寻找第一个空 B 方路线起点。
        for (const auto& deployment : player.deployments)
        {
            occupiedStarts.insert(deployment.first);
        }

        // 此代码块优先把备用区第一个单位部署到第一个空路线起点。
        for (const std::optional<core::OwnedUnitId>& reserveSlot
             : player.reserveSlots)
        {
            if (!reserveSlot.has_value())
            {
                continue;
            }
            for (const core::Route& route : map.routes)
            {
                if (route.side == core::MapSide::B
                    && occupiedStarts.count(route.start) == 0)
                {
                    return core::GameCommand{
                        core::MapSide::B,
                        core::MoveToDeploymentCommand{
                            reserveSlot.value(),
                            route.start}};
                }
            }
            break;
        }

        std::size_t routeCount = 0;
        // 此代码块计算当前地图能够容纳的 B 方占位部署数量。
        for (const core::Route& route : map.routes)
        {
            if (route.side == core::MapSide::B)
            {
                ++routeCount;
            }
        }

        // 此代码块只在活动单位少于路线数时购买第一个可负担商品。
        if (player.activeUnits.size() < routeCount)
        {
            const core::ShopState& shop = view.selfShop.value();
            for (std::size_t slot = 0; slot < shop.offers.size(); ++slot)
            {
                if (shop.offers[slot].has_value()
                    && shop.offers[slot]->displayedPrice <= player.gold)
                {
                    return core::GameCommand{
                        core::MapSide::B,
                        core::PurchaseUnitCommand{slot}};
                }
            }
        }

        return std::nullopt;
    }

    // 此函数为三种策略返回稳定且存在于正式配置中的分队 ID。
    const char* BootstrapComputerController::factionFor(
        const core::AiStrategyKind strategy) noexcept
    {
        // 此代码块让进攻、防守和路线策略分别使用对应主题分队。
        if (strategy == core::AiStrategyKind::Offensive)
        {
            return "assault_team";
        }
        if (strategy == core::AiStrategyKind::Defensive)
        {
            return "training_team";
        }
        return "route_team";
    }
}
