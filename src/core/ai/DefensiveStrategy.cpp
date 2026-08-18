#include "core/ai/DefensiveStrategy.hpp"

#include "core/ai/AiDecisionSupport.hpp"

#include <algorithm>
#include <string>

namespace autochess::core
{
    namespace
    {
        // 此函数为医师加入固定优先级并计算防守属性评分。
        double offerScore(const AiOfferCandidate& candidate)
        {
            const double healerBonus = candidate.definition.basicAction
                    == BasicAction::Heal
                ? 80.0
                : 0.0;
            return candidate.levelOneStats.maxHealth
                + candidate.levelOneStats.physicalDefense * 8.0
                + candidate.levelOneStats.magicResistance * 5.0
                + healerBonus;
        }

        // 此函数保证防守型商品评分相同时选择更便宜且 ID 稳定的商品。
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

        // 此函数提取路线 ID 的下划线后缀用于匹配双方同一条通道。
        std::string routeSuffix(const std::string& routeId)
        {
            const std::size_t separator = routeId.find('_');
            return separator == std::string::npos
                ? routeId
                : routeId.substr(separator + 1);
        }

        // 此函数按对手最短路线优先排列防守部署路线。
        bool mainThreatFirst(
            const AiRouteCandidate& left,
            const AiRouteCandidate& right,
            const ReadOnlyGameView& view,
            const MapSide actor)
        {
            const MapSide opponent = actor == MapSide::A
                ? MapSide::B
                : MapSide::A;
            auto threatLength = [&view, opponent](
                                    const AiRouteCandidate& own)
            {
                std::size_t best = static_cast<std::size_t>(-1);
                const std::string suffix = routeSuffix(own.route.id);
                if (!view.selectedMap.has_value())
                {
                    return best;
                }
                for (const Route& route : view.selectedMap->routes)
                {
                    if (route.side == opponent
                        && routeSuffix(route.id) == suffix)
                    {
                        best = AiDecisionSupport::routeLength(route);
                        break;
                    }
                }
                return best;
            };

            const std::size_t leftThreat = threatLength(left);
            const std::size_t rightThreat = threatLength(right);
            if (leftThreat != rightThreat)
            {
                return leftThreat < rightThreat;
            }
            return left.configIndex < right.configIndex;
        }

        // 此函数将治疗、增益和伤害技能按防守用途排序。
        int skillPriority(
            const BattleUnit& unit,
            const ConfigBundle& config)
        {
            const SkillDefinition* skill = AiDecisionSupport::findSkill(
                config,
                unit.skillId);
            if (skill == nullptr)
            {
                return 0;
            }
            if (skill->effectType == SkillEffectType::Heal)
            {
                return 300;
            }
            if (skill->effectType == SkillEffectType::Buff)
            {
                return 200;
            }
            return 100;
        }
    }

    AiStrategyKind DefensiveStrategy::kind() const noexcept
    {
        // 此返回值标识当前对象采用防守型策略。
        return AiStrategyKind::Defensive;
    }

    const char* DefensiveStrategy::defaultFactionId() const noexcept
    {
        // 此返回值把防守型策略映射到正式坚壁分队。
        return "training_team";
    }

    std::optional<GameCommand> DefensiveStrategy::choosePreparationCommand(
        const MapSide actor,
        const ReadOnlyGameView& view,
        const ConfigBundle& config) const
    {
        // 此分支只允许防守型策略在准备阶段生成命令。
        if (view.phase != MatchPhase::Preparation || !view.self.has_value())
        {
            return std::nullopt;
        }
        const std::size_t targetCount = AiDecisionSupport::targetUnitCount(
            actor,
            view,
            config);
        auto routes = AiDecisionSupport::vacantRoutes(actor, view);
        std::sort(
            routes.begin(),
            routes.end(),
            [&view, actor](
                const AiRouteCandidate& left,
                const AiRouteCandidate& right)
            {
                return mainThreatFirst(left, right, view, actor);
            });
        const auto reserveUnit = AiDecisionSupport::firstReserveUnit(view);
        // 此分支先把备用单位部署到对手最短路线对应的通道。
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

        // 此分支在未达到目标持有数量时购买防守评分最高的商品。
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

    std::optional<GameCommand> DefensiveStrategy::chooseCombatCommand(
        const MapSide actor,
        const ReadOnlyGameView& view,
        const ConfigBundle& config) const
    {
        // 此分支只允许防守型策略在战斗阶段寻找主动技能。
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
        std::sort(
            ready.begin(),
            ready.end(),
            [&config](const BattleUnit& left, const BattleUnit& right)
            {
                const int leftPriority = skillPriority(left, config);
                const int rightPriority = skillPriority(right, config);
                if (leftPriority != rightPriority)
                {
                    return leftPriority > rightPriority;
                }
                return left.id < right.id;
            });
        return GameCommand{
            actor,
            ReleaseSkillCommand{ready.front().id}};
    }
}
