#include "core/ai/OffensiveStrategy.hpp"

#include "core/ai/AiDecisionSupport.hpp"

#include <algorithm>

namespace autochess::core
{
    namespace
    {
        // 此函数把攻击力、攻速、守卫伤害和移动速度合成为进攻评分。
        double offerScore(const AiOfferCandidate& candidate)
        {
            return candidate.levelOneStats.attackPower * 10.0
                + candidate.levelOneStats.attackSpeed * 0.2
                + candidate.levelOneStats.guardDamage * 20.0
                + candidate.levelOneStats.moveSpeed;
        }

        // 此函数保证进攻型商品评分相同时选择更便宜且 ID 稳定的商品。
        bool betterOffer(
            const AiOfferCandidate& left,
            const AiOfferCandidate& right)
        {
            const double leftScore = offerScore(left);
            const double rightScore = offerScore(right);
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

        // 此函数按路线线段数量从短到长排列进攻型部署目标。
        bool shorterRoute(
            const AiRouteCandidate& left,
            const AiRouteCandidate& right)
        {
            if (left.length != right.length)
            {
                return left.length < right.length;
            }
            return left.configIndex < right.configIndex;
        }

        // 此函数把可释放的己方技能按攻击收益稳定排序。
        bool strongerCaster(
            const BattleUnit& left,
            const BattleUnit& right)
        {
            const double leftScore = left.stats.attackPower * 10.0
                + left.stats.attackSpeed * 0.2
                + left.stats.guardDamage * 20.0;
            const double rightScore = right.stats.attackPower * 10.0
                + right.stats.attackSpeed * 0.2
                + right.stats.guardDamage * 20.0;
            if (leftScore != rightScore)
            {
                return leftScore > rightScore;
            }
            return left.id < right.id;
        }
    }

    AiStrategyKind OffensiveStrategy::kind() const noexcept
    {
        // 此返回值标识当前对象采用进攻型策略。
        return AiStrategyKind::Offensive;
    }

    const char* OffensiveStrategy::defaultFactionId() const noexcept
    {
        // 此返回值把进攻型策略映射到正式突击分队。
        return "assault_team";
    }

    std::optional<GameCommand> OffensiveStrategy::choosePreparationCommand(
        const MapSide actor,
        const ReadOnlyGameView& view,
        const ConfigBundle& config) const
    {
        // 此分支只允许进攻型策略在准备阶段生成命令。
        if (view.phase != MatchPhase::Preparation || !view.self.has_value())
        {
            return std::nullopt;
        }

        const std::size_t targetCount = AiDecisionSupport::targetUnitCount(
            actor,
            view,
            config);
        auto routes = AiDecisionSupport::vacantRoutes(actor, view);
        std::sort(routes.begin(), routes.end(), shorterRoute);
        const auto reserveUnit = AiDecisionSupport::firstReserveUnit(view);
        // 此分支先把已经购买的备用单位部署到最短空闲路线。
        if (reserveUnit.has_value()
            && view.self->deployments.size() < targetCount
            && !routes.empty())
        {
            return GameCommand{
                actor,
                MoveToDeploymentCommand{
                    reserveUnit.value(),
                    routes.front().route.start}};
        }

        // 此分支在未达到目标持有数量时购买评分最高的可负担商品。
        if (view.self->activeUnits.size() >= targetCount)
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
            [](const AiOfferCandidate& left, const AiOfferCandidate& right)
            {
                return betterOffer(right, left);
            });
        return GameCommand{
            actor,
            PurchaseUnitCommand{selected->shopSlot}};
    }

    std::optional<GameCommand> OffensiveStrategy::chooseCombatCommand(
        const MapSide actor,
        const ReadOnlyGameView& view,
        const ConfigBundle& config) const
    {
        // 此分支只允许进攻型策略在战斗阶段寻找主动技能。
        if (view.phase != MatchPhase::Combat)
        {
            return std::nullopt;
        }
        std::vector<BattleUnit> ready;
        // 此循环复制当前能够合法释放技能的己方战斗单位。
        for (const BattleUnit& unit : view.battleUnits)
        {
            if (unit.side == actor
                && AiDecisionSupport::skillCanRelease(unit, view, config))
            {
                ready.push_back(unit);
            }
        }
        if (ready.empty())
        {
            return std::nullopt;
        }
        std::sort(ready.begin(), ready.end(), strongerCaster);
        return GameCommand{
            actor,
            ReleaseSkillCommand{ready.front().id}};
    }
}
