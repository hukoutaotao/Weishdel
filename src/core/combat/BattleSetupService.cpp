#include "core/combat/BattleSetupService.hpp"

#include "core/model/PlayerStateService.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace autochess::core
{
    namespace
    {
        constexpr double kMinimumStat = 0.0;

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

        bool resolveStats(
            const UnitDefinition& definition,
            const int level,
            BattleStats& stats,
            std::string& errorMessage)
        {
            if (level < 1 || level > 3)
            {
                return fail(errorMessage, "战斗单位等级必须在 1 到 3 之间");
            }

            const double multiplier =
                definition.levelMultipliers[static_cast<std::size_t>(level - 1)];
            if (multiplier <= kMinimumStat)
            {
                return fail(errorMessage, "单位等级倍率必须为正数");
            }

            stats.maxHealth =
                static_cast<double>(definition.maxHealth) * multiplier;
            stats.attackPower =
                static_cast<double>(definition.attackPower) * multiplier;
            stats.physicalDefense =
                static_cast<double>(definition.physicalDefense) * multiplier;
            stats.magicResistance = std::clamp(
                static_cast<double>(definition.magicResistance) * multiplier,
                0.0,
                100.0);
            stats.attackRange = definition.attackRange * multiplier;
            stats.moveSpeed = definition.moveSpeed * multiplier;
            stats.attackSpeed = std::clamp(
                static_cast<double>(definition.attackSpeed) * multiplier,
                0.0,
                600.0);
            stats.guardDamage = static_cast<int>(std::round(
                static_cast<double>(definition.guardDamage) * multiplier));

            if (stats.maxHealth <= kMinimumStat
                || stats.attackRange < kMinimumStat
                || stats.moveSpeed < kMinimumStat
                || stats.guardDamage < 0)
            {
                return fail(errorMessage, "单位战斗属性必须满足非负约束");
            }

            return true;
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
                if (!resolveStats(
                        *definition,
                        ownedUnit->identity.level,
                        battleUnit.stats,
                        errorMessage))
                {
                    return false;
                }

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

        std::vector<BattleUnit> created;
        BattleUnitId nextBattleUnitId = 1;
        if (!appendPlayerUnits(
                playerA,
                map,
                definitions,
                nextBattleUnitId,
                created,
                errorMessage)
            || !appendPlayerUnits(
                playerB,
                map,
                definitions,
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
