#include "core/combat/BattleSetupService.hpp"

#include "core/combat/BattleStatResolver.hpp"
#include "core/model/PlayerStateService.hpp"

#include <algorithm>
#include <utility>

namespace autochess::core
{
    namespace
    {
        bool fail(std::string& errorMessage, const char* message)
        {
            errorMessage = message;
            return false;
        }

        BattlePosition centerOf(const GridPosition position)
        {
            return BattlePosition{
                static_cast<double>(position.x) + 0.5,
                static_cast<double>(position.y) + 0.5};
        }

        const Route* findRoute(
            const MapDefinition& map,
            const MapSide side,
            const GridPosition start)
        {
            const Route* found = nullptr;
            for (const Route& route : map.routes)
            {
                if (route.side != side || route.start != start)
                {
                    continue;
                }

                if (found != nullptr)
                {
                    return nullptr;
                }

                found = &route;
            }

            return found;
        }

        const UnitDefinition* findDefinition(
            const std::vector<UnitDefinition>& definitions,
            const std::string& unitId)
        {
            const auto iterator = std::find_if(
                definitions.begin(),
                definitions.end(),
                [&unitId](const UnitDefinition& definition)
                {
                    return definition.id == unitId;
                });

            return iterator == definitions.end() ? nullptr : &*iterator;
        }

        // 此函数按玩家保存的分队 ID 查找唯一分队定义。
        const FactionDefinition* findFaction(
            const std::vector<FactionDefinition>& factions,
            const std::string& factionId)
        {
            const auto iterator = std::find_if(
                factions.begin(),
                factions.end(),
                [&factionId](const FactionDefinition& faction)
                {
                    return faction.id == factionId;
                });
            return iterator == factions.end() ? nullptr : &*iterator;
        }

        bool validatePlayer(
            const PlayerState& player,
            const MapSide expectedSide,
            std::string& errorMessage)
        {
            if (player.side != expectedSide)
            {
                return fail(errorMessage, "战斗初始化要求双方分别属于 A、B 阵营");
            }

            if (!PlayerStateService::validate(player, errorMessage))
            {
                return false;
            }

            return true;
        }

        bool appendPlayerUnits(
            const PlayerState& player,
            const MapDefinition& map,
            const std::vector<UnitDefinition>& definitions,
            const FactionDefinition& faction,
            const std::vector<FactionModifierDefinition>& modifiers,
            BattleUnitId& nextBattleUnitId,
            std::vector<BattleUnit>& output,
            std::string& errorMessage)
        {
            for (const auto& deployment : player.deployments)
            {
                const OwnedUnit* ownedUnit =
                    PlayerStateService::findActive(player, deployment.second);
                if (ownedUnit == nullptr)
                {
                    return fail(errorMessage, "部署映射引用了不存在的活动单位");
                }

                const Route* route = findRoute(map, player.side, deployment.first);
                if (route == nullptr || route->points.empty())
                {
                    return fail(errorMessage, "部署单位缺少唯一的有效路线");
                }

                const UnitDefinition* definition =
                    findDefinition(definitions, ownedUnit->identity.unitId);
                if (definition == nullptr)
                {
                    return fail(errorMessage, "部署单位缺少对应的单位定义");
                }

                BattleUnit battleUnit;
                battleUnit.id = nextBattleUnitId++;
                battleUnit.ownedUnitId = ownedUnit->id;
                battleUnit.identity = ownedUnit->identity;
                battleUnit.side = ownedUnit->ownerSide;
                battleUnit.basicAction = definition->basicAction;
                battleUnit.basicDamageType = definition->basicDamageType;
                if (!BattleStatResolver::resolve(
                        *definition,
                        ownedUnit->identity.level,
                        faction,
                        modifiers,
                        battleUnit.stats,
                        errorMessage))
                {
                    return false;
                }

                // 此代码块保存技能前的基础属性和配置驱动的初始技力状态。
                battleUnit.baseStats = battleUnit.stats;
                battleUnit.skillId = definition->skillId;
                battleUnit.currentMana =
                    static_cast<double>(definition->initialMana);
                battleUnit.maxMana = static_cast<double>(definition->maxMana);
                battleUnit.position = centerOf(route->points.front());
                battleUnit.routePoints = route->points;
                battleUnit.nextRoutePointIndex =
                    battleUnit.routePoints.size() > 1 ? 1 : 0;
                battleUnit.health = battleUnit.stats.maxHealth;
                output.push_back(std::move(battleUnit));
            }

            return true;
        }
    }

    bool BattleSetupService::createUnits(
        const PlayerState& playerA,
        const PlayerState& playerB,
        const MapDefinition& map,
        const std::vector<UnitDefinition>& definitions,
        const std::vector<FactionDefinition>& factions,
        const std::vector<FactionModifierDefinition>& modifiers,
        std::vector<BattleUnit>& output,
        std::string& errorMessage)
    {
        errorMessage.clear();

        if (map.width <= 0 || map.height <= 0 || map.gridRows.size()
                != static_cast<std::size_t>(map.height))
        {
            return fail(errorMessage, "战斗初始化收到无效地图尺寸");
        }

        if (!validatePlayer(playerA, MapSide::A, errorMessage)
            || !validatePlayer(playerB, MapSide::B, errorMessage))
        {
            return false;
        }

        // 此代码块为双方分别解析分队，允许同一场战斗使用不同分队修正。
        const FactionDefinition* factionA =
            findFaction(factions, playerA.factionId);
        const FactionDefinition* factionB =
            findFaction(factions, playerB.factionId);
        if (factionA == nullptr || factionB == nullptr)
        {
            return fail(errorMessage, "玩家分队缺少对应的分队定义");
        }

        std::vector<BattleUnit> created;
        BattleUnitId nextBattleUnitId = 1;
        if (!appendPlayerUnits(
                playerA,
                map,
                definitions,
                *factionA,
                modifiers,
                nextBattleUnitId,
                created,
                errorMessage)
            || !appendPlayerUnits(
                playerB,
                map,
                definitions,
                *factionB,
                modifiers,
                nextBattleUnitId,
                created,
                errorMessage))
        {
            return false;
        }

        output = std::move(created);
        return true;
    }
}
