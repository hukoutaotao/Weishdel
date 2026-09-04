#include "core/ai/RouteStrategy.hpp"

#include "core/ai/AiDecisionSupport.hpp"
#include "core/combat/BattleStatResolver.hpp"

#include <algorithm>
#include <limits>

namespace autochess::core
{
    namespace
    {
        // 此函数按预计移动时间、射程和守卫伤害评价一个商品路线组合。
        double routeScore(
            const AiOfferCandidate& offer,
            const AiRouteCandidate& route)
        {
            const double speed = std::max(
                offer.levelOneStats.moveSpeed,
                0.01);
            const double travelSeconds =
                static_cast<double>(route.length) / speed;
            return offer.levelOneStats.guardDamage * 100.0
                + offer.levelOneStats.attackRange * 10.0
                + offer.levelOneStats.moveSpeed * 10.0
                - travelSeconds * 20.0;
        }

        // 此函数返回一个商品在当前空闲路线上的最佳路线评分。
        double bestOfferScore(
            const AiOfferCandidate& offer,
            const std::vector<AiRouteCandidate>& routes)
        {
            double score = -std::numeric_limits<double>::infinity();
            for (const AiRouteCandidate& route : routes)
            {
                score = std::max(score, routeScore(offer, route));
            }
            return score;
        }

        // 此函数保证路线评分相同时选择更便宜且 ID 稳定的商品。
        bool betterOffer(
            const AiOfferCandidate& left,
            const AiOfferCandidate& right,
            const std::vector<AiRouteCandidate>& routes)
        {
            const double leftScore = bestOfferScore(left, routes);
            const double rightScore = bestOfferScore(right, routes);
            if (leftScore != rightScore)
            {
                return leftScore > rightScore;
            }
            if (left.offer.displayedPrice != right.offer.displayedPrice)
            {
                return left.offer.displayedPrice
                    < right.offer.displayedPrice;
            }
            if (left.offer.unitId != right.offer.unitId)
            {
                return left.offer.unitId < right.offer.unitId;
            }
            return left.shopSlot < right.shopSlot;
        }

        // 此函数为已有单位选择预计移动时间最短的路线。
        bool betterRoute(
            const AiRouteCandidate& left,
            const AiRouteCandidate& right,
            const BattleStats& stats)
        {
            const double speed = std::max(stats.moveSpeed, 0.01);
            const double leftTime = left.length / speed;
            const double rightTime = right.length / speed;
            if (leftTime != rightTime)
            {
                return leftTime < rightTime;
            }
            return left.configIndex < right.configIndex;
        }

    }

    AiStrategyKind RouteStrategy::kind() const noexcept
    {
        // 此返回值标识当前对象采用路线型策略。
        return AiStrategyKind::Route;
    }

    const char* RouteStrategy::defaultFactionId() const noexcept
    {
        // 此返回值把路线型策略映射到正式行军分队。
        return "route_team";
    }

    std::optional<GameCommand> RouteStrategy::choosePreparationCommand(
        const MapSide actor,
        const ReadOnlyGameView& view,
        const ConfigBundle& config) const
    {
        // 此分支只允许路线型策略在准备阶段生成命令。
        if (view.phase != MatchPhase::Preparation || !view.self.has_value())
        {
            return std::nullopt;
        }
        const std::size_t targetCount = AiDecisionSupport::targetUnitCount(
            actor,
            view,
            config);
        const auto routes = AiDecisionSupport::vacantRoutes(actor, view);
        const auto reserveUnit = AiDecisionSupport::firstReserveUnit(view);
        // 此分支根据备用单位属性为其选择预计移动时间最短的路线。
        if (reserveUnit.has_value()
            && view.self->deployments.size() < targetCount
            && !routes.empty())
        {
            const auto owned = std::find_if(
                view.self->activeUnits.begin(),
                view.self->activeUnits.end(),
                [reserveUnit](const OwnedUnit& unit)
                {
                    return unit.id == reserveUnit.value();
                });
            const UnitDefinition* definition = owned
                == view.self->activeUnits.end()
                ? nullptr
                : AiDecisionSupport::findUnit(
                      config,
                      owned->identity.unitId);
            const FactionDefinition* faction = AiDecisionSupport::findFaction(
                config,
                view.self->factionId);
            if (definition != nullptr && faction != nullptr)
            {
                BattleStats stats;
                std::string errorMessage;
                if (BattleStatResolver::resolve(
                        *definition,
                        owned->identity.level,
                        *faction,
                        config.factionModifiers,
                        stats,
                        errorMessage))
                {
                    auto orderedRoutes = routes;
                    std::sort(
                        orderedRoutes.begin(),
                        orderedRoutes.end(),
                        [&stats](
                            const AiRouteCandidate& left,
                            const AiRouteCandidate& right)
                        {
                            return betterRoute(left, right, stats);
                        });
                    return GameCommand{
                        actor,
                        MoveToDeploymentCommand{
                            reserveUnit.value(),
                            orderedRoutes.front().route.start}};
                }
            }
        }

        // 此分支在未达到目标持有数量时购买路线综合评分最高的商品。
        if (view.self->activeUnits.size() >= targetCount || routes.empty())
        {
            return std::nullopt;
        }
        auto offers = AiDecisionSupport::affordableOffers(view, config);
        if (offers.empty())
        {
            return std::nullopt;
        }
        const auto selected = std::max_element(
            offers.begin(),
            offers.end(),
            [&routes](
                const AiOfferCandidate& left,
                const AiOfferCandidate& right)
            {
                return betterOffer(right, left, routes);
            });
        return GameCommand{
            actor,
            PurchaseUnitCommand{selected->shopSlot}};
    }
}
