#include "core/ai/AiDecisionSupport.hpp"

#include "core/combat/BattleStatResolver.hpp"

#include <algorithm>

namespace autochess::core
{
    const UnitDefinition* AiDecisionSupport::findUnit(
        const ConfigBundle& config,
        const std::string& unitId) noexcept
    {
        // 此循环只读取正式配置并返回匹配单位的稳定地址。
        const auto iterator = std::find_if(
            config.units.begin(),
            config.units.end(),
            [&unitId](const UnitDefinition& definition)
            {
                return definition.id == unitId;
            });
        return iterator == config.units.end() ? nullptr : &*iterator;
    }

    const FactionDefinition* AiDecisionSupport::findFaction(
        const ConfigBundle& config,
        const std::string& factionId) noexcept
    {
        // 此循环只读取正式分队配置并返回匹配分队的稳定地址。
        const auto iterator = std::find_if(
            config.factions.begin(),
            config.factions.end(),
            [&factionId](const FactionDefinition& definition)
            {
                return definition.id == factionId;
            });
        return iterator == config.factions.end() ? nullptr : &*iterator;
    }

    std::vector<AiOfferCandidate> AiDecisionSupport::affordableOffers(
        const ReadOnlyGameView& view,
        const ConfigBundle& config)
    {
        std::vector<AiOfferCandidate> candidates;
        // 此分支保证缺少自身或商店快照时不生成任何购买命令。
        if (!view.self.has_value() || !view.selfShop.has_value())
        {
            return candidates;
        }

        const FactionDefinition* faction = findFaction(
            config,
            view.self->factionId);
        // 此分支拒绝没有有效分队定义的异常快照。
        if (faction == nullptr)
        {
            return candidates;
        }

        // 此循环只收集存在、可负担且能解析战斗属性的商店商品。
        for (std::size_t slot = 0; slot < view.selfShop->offers.size(); ++slot)
        {
            if (!view.selfShop->offers[slot].has_value())
            {
                continue;
            }
            const ShopOffer& offer = view.selfShop->offers[slot].value();
            if (offer.displayedPrice <= 0
                || offer.displayedPrice > view.self->gold)
            {
                continue;
            }
            const UnitDefinition* definition = findUnit(config, offer.unitId);
            if (definition == nullptr)
            {
                continue;
            }

            BattleStats stats;
            std::string errorMessage;
            if (!BattleStatResolver::resolve(
                    *definition,
                    1,
                    *faction,
                    config.factionModifiers,
                    stats,
                    errorMessage))
            {
                continue;
            }
            candidates.push_back(AiOfferCandidate{
                slot,
                offer,
                *definition,
                stats});
        }
        return candidates;
    }

    std::vector<AiRouteCandidate> AiDecisionSupport::vacantRoutes(
        const MapSide side,
        const ReadOnlyGameView& view)
    {
        std::vector<AiRouteCandidate> candidates;
        // 此分支保证缺少地图或自身状态时不产生部署目标。
        if (!view.selectedMap.has_value() || !view.self.has_value())
        {
            return candidates;
        }

        // 此循环筛选己方路线并排除已经占用的部署起点。
        for (std::size_t index = 0;
             index < view.selectedMap->routes.size();
             ++index)
        {
            const Route& route = view.selectedMap->routes[index];
            if (route.side != side
                || view.self->deployments.count(route.start) != 0)
            {
                continue;
            }
            candidates.push_back(AiRouteCandidate{
                index,
                route,
                routeLength(route)});
        }
        return candidates;
    }

    std::size_t AiDecisionSupport::targetUnitCount(
        const MapSide side,
        const ReadOnlyGameView& view,
        const ConfigBundle& config) noexcept
    {
        // 此分支在缺少地图、分队或自身状态时返回零，避免越权决策。
        if (!view.selectedMap.has_value() || !view.self.has_value())
        {
            return 0;
        }
        const FactionDefinition* faction = findFaction(
            config,
            view.self->factionId);
        if (faction == nullptr)
        {
            return 0;
        }

        std::size_t routeCount = 0;
        // 此循环统计当前地图属于指定阵营的合法路线数量。
        for (const Route& route : view.selectedMap->routes)
        {
            if (route.side == side)
            {
                ++routeCount;
            }
        }

        const std::size_t capacity = config.gameConfig.rosterCapacity > 0
            ? static_cast<std::size_t>(config.gameConfig.rosterCapacity)
            : 0U;
        const std::size_t deployedLimit = faction->maxDeployed > 0
            ? static_cast<std::size_t>(faction->maxDeployed)
            : 0U;
        return std::min(capacity, std::min(deployedLimit, routeCount));
    }

    std::optional<OwnedUnitId> AiDecisionSupport::firstReserveUnit(
        const ReadOnlyGameView& view) noexcept
    {
        // 此分支保证缺少自身状态时不返回伪造的持久 ID。
        if (!view.self.has_value())
        {
            return std::nullopt;
        }
        // 此循环按照备用区的固定顺序选择第一个真实单位。
        for (const std::optional<OwnedUnitId>& unitId
             : view.self->reserveSlots)
        {
            if (unitId.has_value() && unitId.value() != InvalidOwnedUnitId)
            {
                return unitId;
            }
        }
        return std::nullopt;
    }

    std::size_t AiDecisionSupport::routeLength(
        const Route& route) noexcept
    {
        // 此表达式把连续路线的点数转换为需要移动的线段数量。
        return route.points.size() > 1 ? route.points.size() - 1 : 0U;
    }
}
