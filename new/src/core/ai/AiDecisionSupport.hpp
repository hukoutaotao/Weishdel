#pragma once

#include "core/combat/BattleTypes.hpp"
#include "core/config/ConfigBundleLoader.hpp"
#include "core/match/ReadOnlyGameView.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// 此命名空间保存三种策略共用的只读决策资料。
namespace autochess::core
{
    // 此结构把一个可购买商品与其一级有效战斗属性绑定。
    struct AiOfferCandidate
    {
        std::size_t shopSlot = 0;
        ShopOffer offer;
        UnitDefinition definition;
        BattleStats levelOneStats;
    };

    // 此结构保存路线副本及其在地图配置中的稳定顺序。
    struct AiRouteCandidate
    {
        std::size_t configIndex = 0;
        Route route;
        std::size_t length = 0;
    };

    // 此类集中处理 AI 不应各自重复实现的快照查询和规则判断。
    class AiDecisionSupport
    {
    public:
        // 此函数按 ID 查找一个单位定义，找不到时返回空指针。
        static const UnitDefinition* findUnit(
            const ConfigBundle& config,
            const std::string& unitId) noexcept;

        // 此函数按 ID 查找一个分队定义，找不到时返回空指针。
        static const FactionDefinition* findFaction(
            const ConfigBundle& config,
            const std::string& factionId) noexcept;

        // 此函数返回当前快照中价格可负担且属性可解析的商品。
        static std::vector<AiOfferCandidate> affordableOffers(
            const ReadOnlyGameView& view,
            const ConfigBundle& config);

        // 此函数返回指定阵营的空闲合法部署路线。
        static std::vector<AiRouteCandidate> vacantRoutes(
            MapSide side,
            const ReadOnlyGameView& view);

        // 此函数返回指定分队和地图允许的目标持有数量。
        static std::size_t targetUnitCount(
            MapSide side,
            const ReadOnlyGameView& view,
            const ConfigBundle& config) noexcept;

        // 此函数返回备用区中第一个可部署的持久单位 ID。
        static std::optional<OwnedUnitId> firstReserveUnit(
            const ReadOnlyGameView& view) noexcept;

        // 此函数计算路线点数量对应的移动段数。
        static std::size_t routeLength(const Route& route) noexcept;
    };
}
