#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace autochess::core
{
    enum class BasicAction
    {
        Unknown,
        Attack,
        Heal
    };

    enum class DamageType
    {
        Unknown,
        None,
        Physical,
        Magic
    };

    enum class SkillEffectType
    {
        Unknown,
        Damage,
        Heal,
        Buff
    };

    enum class SkillTargetRule
    {
        Unknown,
        Self,
        Enemy,
        LowestHealthAlly
    };

    enum class BuffStat
    {
        Unknown,
        None,
        AttackPower,
        PhysicalDefense,
        MagicResistance,
        MoveSpeed,
        AttackSpeed
    };

    enum class ModifierMode
    {
        Unknown,
        None,
        Add,
        Multiply
    };

    enum class FactionAttribute
    {
        Unknown,
        MaxHealth,
        AttackPower,
        PhysicalDefense,
        MagicResistance,
        AttackRange,
        MoveSpeed,
        AttackSpeed,
        GuardDamage,
        Price
    };

    enum class FactionOperation
    {
        Unknown,
        Add,
        Multiply
    };

    struct GameConfig
    {
        int maxRounds = 0;
        int preparationSeconds = 0;
        int combatTimeoutSeconds = 0;
        int startingGold = 0;
        int roundIncome = 0;
        int loserBonus = 0;
        int shopSlots = 0;
        int shopRefreshCost = 0;
        int rosterCapacity = 0;
        double sellRatio = 0.0;
        double mergeRefundRatio = 0.0;
        double reviveRatio = 0.0;
        int maxUnitLevel = 0;
        std::uint32_t randomSeed = 0;
    };

    struct UnitDefinition
    {
        std::string id;
        std::string name;
        std::vector<std::string> tags;

        int maxHealth = 0;
        int attackPower = 0;
        int physicalDefense = 0;
        int magicResistance = 0;

        double attackRange = 0.0;
        double moveSpeed = 0.0;
        int attackSpeed = 0;

        int guardDamage = 0;
        int price = 0;
        int initialMana = 0;
        int maxMana = 0;

        BasicAction basicAction = BasicAction::Unknown;
        DamageType basicDamageType = DamageType::Unknown;
        std::string skillId;
        std::array<double, 3> levelMultipliers{};
    };

    struct SkillDefinition
    {
        std::string id;
        std::string name;
        std::string description;

        SkillEffectType effectType = SkillEffectType::Unknown;
        SkillTargetRule targetRule = SkillTargetRule::Unknown;
        int targetCount = 0;
        double effectRange = 0.0;
        DamageType damageType = DamageType::Unknown;

        std::array<double, 3> levelValues{};
        double durationSeconds = 0.0;

        BuffStat buffStat = BuffStat::Unknown;
        ModifierMode modifierMode = ModifierMode::Unknown;

        bool allowSelf = false;
        bool allowMove = false;
        bool allowBasicAction = false;
    };

    struct FactionDefinition
    {
        std::string id;
        std::string name;
        int initialGuard = 0;
        int maxDeployed = 0;
        double priceMultiplier = 0.0;
    };

    struct FactionModifierDefinition
    {
        std::string id;
        std::string factionId;
        std::string unitId;
        FactionAttribute attribute = FactionAttribute::Unknown;
        FactionOperation operation = FactionOperation::Unknown;
        double value = 0.0;
    };
}
