#include "core/config/DefinitionConfigLoader.hpp"

#include "core/config/ConfigParser.hpp"
#include "core/config/ConfigValueReader.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace autochess::core
{
    namespace
    {
        // 此函数返回技能节允许出现的冻结字段集合。
        const std::set<std::string>& skillAllowedFields()
        {
            static const std::set<std::string> fields = {
                "name",
                "description",
                "effect_type",
                "target_rule",
                "target_count",
                "effect_range",
                "damage_type",
                "level_values",
                "duration_seconds",
                "buff_stat",
                "modifier_mode",
                "allow_self",
                "allow_move",
                "allow_basic_action"};
            return fields;
        }

        // 此函数返回单位节允许出现的冻结字段集合。
        const std::set<std::string>& unitAllowedFields()
        {
            static const std::set<std::string> fields = {
                "name",
                "tags",
                "max_health",
                "attack_power",
                "physical_defense",
                "magic_resistance",
                "attack_range",
                "move_speed",
                "attack_speed",
                "guard_damage",
                "price",
                "initial_mana",
                "max_mana",
                "basic_action",
                "basic_damage_type",
                "skill_id",
                "level_multipliers"};
            return fields;
        }

        // 此函数返回分队节允许出现的冻结字段集合。
        const std::set<std::string>& factionAllowedFields()
        {
            static const std::set<std::string> fields = {
                "name",
                "initial_guard",
                "max_deployed",
                "price_multiplier"};
            return fields;
        }

        // 此函数返回分队修正节允许出现的冻结字段集合。
        const std::set<std::string>& modifierAllowedFields()
        {
            static const std::set<std::string> fields = {
                "faction_id",
                "unit_id",
                "attribute",
                "operation",
                "value"};
            return fields;
        }

        // 此函数把文本转换为普通行动枚举。
        bool parseBasicAction(const std::string& text, BasicAction& value)
        {
            if (text == "attack")
            {
                value = BasicAction::Attack;
                return true;
            }
            if (text == "heal")
            {
                value = BasicAction::Heal;
                return true;
            }
            return false;
        }

        // 此函数把文本转换为伤害类型枚举。
        bool parseDamageType(const std::string& text, DamageType& value)
        {
            if (text == "none")
            {
                value = DamageType::None;
                return true;
            }
            if (text == "physical")
            {
                value = DamageType::Physical;
                return true;
            }
            if (text == "magic")
            {
                value = DamageType::Magic;
                return true;
            }
            return false;
        }

        // 此函数把文本转换为技能效果类型枚举。
        bool parseSkillEffectType(
            const std::string& text,
            SkillEffectType& value)
        {
            if (text == "damage")
            {
                value = SkillEffectType::Damage;
                return true;
            }
            if (text == "heal")
            {
                value = SkillEffectType::Heal;
                return true;
            }
            if (text == "buff")
            {
                value = SkillEffectType::Buff;
                return true;
            }
            return false;
        }

        // 此函数把文本转换为技能目标规则枚举。
        bool parseSkillTargetRule(
            const std::string& text,
            SkillTargetRule& value)
        {
            if (text == "self")
            {
                value = SkillTargetRule::Self;
                return true;
            }
            if (text == "enemy")
            {
                value = SkillTargetRule::Enemy;
                return true;
            }
            if (text == "lowest_health_ally")
            {
                value = SkillTargetRule::LowestHealthAlly;
                return true;
            }
            return false;
        }

        // 此函数把文本转换为增益属性枚举。
        bool parseBuffStat(const std::string& text, BuffStat& value)
        {
            if (text == "none")
            {
                value = BuffStat::None;
                return true;
            }
            if (text == "attack_power")
            {
                value = BuffStat::AttackPower;
                return true;
            }
            if (text == "physical_defense")
            {
                value = BuffStat::PhysicalDefense;
                return true;
            }
            if (text == "magic_resistance")
            {
                value = BuffStat::MagicResistance;
                return true;
            }
            if (text == "move_speed")
            {
                value = BuffStat::MoveSpeed;
                return true;
            }
            if (text == "attack_speed")
            {
                value = BuffStat::AttackSpeed;
                return true;
            }
            return false;
        }

        // 此函数把文本转换为技能修正方式枚举。
        bool parseModifierMode(const std::string& text, ModifierMode& value)
        {
            if (text == "none")
            {
                value = ModifierMode::None;
                return true;
            }
            if (text == "add")
            {
                value = ModifierMode::Add;
                return true;
            }
            if (text == "multiply")
            {
                value = ModifierMode::Multiply;
                return true;
            }
            return false;
        }

        // 此函数把文本转换为分队可修正属性枚举。
        bool parseFactionAttribute(
            const std::string& text,
            FactionAttribute& value)
        {
            if (text == "max_health")
            {
                value = FactionAttribute::MaxHealth;
                return true;
            }
            if (text == "attack_power")
            {
                value = FactionAttribute::AttackPower;
                return true;
            }
            if (text == "physical_defense")
            {
                value = FactionAttribute::PhysicalDefense;
                return true;
            }
            if (text == "magic_resistance")
            {
                value = FactionAttribute::MagicResistance;
                return true;
            }
            if (text == "attack_range")
            {
                value = FactionAttribute::AttackRange;
                return true;
            }
            if (text == "move_speed")
            {
                value = FactionAttribute::MoveSpeed;
                return true;
            }
            if (text == "attack_speed")
            {
                value = FactionAttribute::AttackSpeed;
                return true;
            }
            if (text == "guard_damage")
            {
                value = FactionAttribute::GuardDamage;
                return true;
            }
            if (text == "price")
            {
                value = FactionAttribute::Price;
                return true;
            }
            return false;
        }

        // 此函数把文本转换为分队修正运算枚举。
        bool parseFactionOperation(
            const std::string& text,
            FactionOperation& value)
        {
            if (text == "add")
            {
                value = FactionOperation::Add;
                return true;
            }
            if (text == "multiply")
            {
                value = FactionOperation::Multiply;
                return true;
            }
            return false;
        }

        // 此函数为无法识别的枚举字段生成带行号的类型错误。
        bool failInvalidEnum(
            const ConfigSection& section,
            const std::filesystem::path& path,
            const std::string& key,
            ConfigError& error)
        {
            return failConfig(
                error,
                ConfigErrorCategory::TypeError,
                path,
                section.fields.at(key).line,
                "字段 " + key + " 包含未知枚举值");
        }

        // 此函数检查指定技能 ID 是否已经加载。
        bool containsSkillId(
            const std::vector<SkillDefinition>& skills,
            const std::string& id)
        {
            return std::any_of(
                skills.begin(),
                skills.end(),
                [&id](const SkillDefinition& skill)
                {
                    return skill.id == id;
                });
        }

        // 此函数检查指定单位 ID 是否已经加载。
        bool containsUnitId(
            const std::vector<UnitDefinition>& units,
            const std::string& id)
        {
            return std::any_of(
                units.begin(),
                units.end(),
                [&id](const UnitDefinition& unit)
                {
                    return unit.id == id;
                });
        }

        // 此函数检查指定分队 ID 是否已经加载。
        bool containsFactionId(
            const std::vector<FactionDefinition>& factions,
            const std::string& id)
        {
            return std::any_of(
                factions.begin(),
                factions.end(),
                [&id](const FactionDefinition& faction)
                {
                    return faction.id == id;
                });
        }

        // 此函数读取并校验一个技能节的全部字段。
        bool parseSkillSection(
            const ConfigSection& section,
            const std::filesystem::path& path,
            SkillDefinition& skill,
            ConfigError& error)
        {
            if (!rejectUnknownConfigFields(
                    section, path, skillAllowedFields(), "skill", error))
            {
                return false;
            }

            // 此代码段把全部必填技能字段读取到局部对象中。
            std::string effectTypeText;
            std::string targetRuleText;
            std::string damageTypeText;
            std::string buffStatText;
            std::string modifierModeText;
            std::vector<double> levelValues;
            if (!readStringConfigField(section, path, "name", skill.name, error)
                || !readStringConfigField(section, path, "description", skill.description, error)
                || !readStringConfigField(section, path, "effect_type", effectTypeText, error)
                || !readStringConfigField(section, path, "target_rule", targetRuleText, error)
                || !readIntConfigField(section, path, "target_count", skill.targetCount, error)
                || !readDoubleConfigField(section, path, "effect_range", skill.effectRange, error)
                || !readStringConfigField(section, path, "damage_type", damageTypeText, error)
                || !readDoubleListConfigField(section, path, "level_values", levelValues, error)
                || !readDoubleConfigField(section, path, "duration_seconds", skill.durationSeconds, error)
                || !readStringConfigField(section, path, "buff_stat", buffStatText, error)
                || !readStringConfigField(section, path, "modifier_mode", modifierModeText, error)
                || !readBoolConfigField(section, path, "allow_self", skill.allowSelf, error)
                || !readBoolConfigField(section, path, "allow_move", skill.allowMove, error)
                || !readBoolConfigField(section, path, "allow_basic_action", skill.allowBasicAction, error))
            {
                return false;
            }

            // 此代码段把技能文本枚举严格转换为定义中的枚举类型。
            if (!parseSkillEffectType(effectTypeText, skill.effectType))
            {
                return failInvalidEnum(section, path, "effect_type", error);
            }
            if (!parseSkillTargetRule(targetRuleText, skill.targetRule))
            {
                return failInvalidEnum(section, path, "target_rule", error);
            }
            if (!parseDamageType(damageTypeText, skill.damageType))
            {
                return failInvalidEnum(section, path, "damage_type", error);
            }
            if (!parseBuffStat(buffStatText, skill.buffStat))
            {
                return failInvalidEnum(section, path, "buff_stat", error);
            }
            if (!parseModifierMode(modifierModeText, skill.modifierMode))
            {
                return failInvalidEnum(section, path, "modifier_mode", error);
            }

            // 此代码段校验技能的通用数值范围和三级数值列表。
            if (!requireConfigRange(skill.targetCount > 0, section, path,
                    "target_count", "必须大于 0", error)
                || !requireConfigRange(skill.effectRange >= 0.0, section, path,
                    "effect_range", "必须大于或等于 0", error)
                || !requireConfigRange(skill.durationSeconds >= 0.0, section, path,
                    "duration_seconds", "必须大于或等于 0", error))
            {
                return false;
            }
            if (levelValues.size() != 3)
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::TypeError,
                    path,
                    section.fields.at("level_values").line,
                    "字段 level_values 必须恰好包含三个小数");
            }
            if (!requireConfigRange(
                    std::all_of(levelValues.begin(), levelValues.end(),
                        [](const double value) { return value >= 0.0; }),
                    section,
                    path,
                    "level_values",
                    "中的所有值必须大于或等于 0",
                    error))
            {
                return false;
            }
            skill.levelValues = {levelValues[0], levelValues[1], levelValues[2]};

            // 此代码段校验伤害、治疗和增益技能各自的条件字段组合。
            if (skill.effectType == SkillEffectType::Damage)
            {
                if (!requireConfigRange(
                        skill.damageType == DamageType::Physical
                            || skill.damageType == DamageType::Magic,
                        section, path, "damage_type",
                        "在伤害技能中必须是 physical 或 magic", error)
                    || !requireConfigRange(skill.durationSeconds == 0.0,
                        section, path, "duration_seconds",
                        "在伤害技能中必须等于 0", error)
                    || !requireConfigRange(skill.buffStat == BuffStat::None,
                        section, path, "buff_stat",
                        "在伤害技能中必须是 none", error)
                    || !requireConfigRange(skill.modifierMode == ModifierMode::None,
                        section, path, "modifier_mode",
                        "在伤害技能中必须是 none", error))
                {
                    return false;
                }
            }
            else if (skill.effectType == SkillEffectType::Heal)
            {
                if (!requireConfigRange(skill.damageType == DamageType::None,
                        section, path, "damage_type",
                        "在治疗技能中必须是 none", error)
                    || !requireConfigRange(skill.durationSeconds == 0.0,
                        section, path, "duration_seconds",
                        "在治疗技能中必须等于 0", error)
                    || !requireConfigRange(skill.buffStat == BuffStat::None,
                        section, path, "buff_stat",
                        "在治疗技能中必须是 none", error)
                    || !requireConfigRange(skill.modifierMode == ModifierMode::None,
                        section, path, "modifier_mode",
                        "在治疗技能中必须是 none", error))
                {
                    return false;
                }
            }
            else
            {
                if (!requireConfigRange(skill.damageType == DamageType::None,
                        section, path, "damage_type",
                        "在增益技能中必须是 none", error)
                    || !requireConfigRange(skill.durationSeconds > 0.0,
                        section, path, "duration_seconds",
                        "在增益技能中必须大于 0", error)
                    || !requireConfigRange(skill.buffStat != BuffStat::None,
                        section, path, "buff_stat",
                        "在增益技能中不得是 none", error)
                    || !requireConfigRange(
                        skill.modifierMode == ModifierMode::Add
                            || skill.modifierMode == ModifierMode::Multiply,
                        section, path, "modifier_mode",
                        "在增益技能中必须是 add 或 multiply", error))
                {
                    return false;
                }
            }

            // 此代码段校验以自身为目标时数量、范围和自选标记的一致性。
            if (skill.targetRule == SkillTargetRule::Self
                && (!requireConfigRange(skill.targetCount == 1, section, path,
                        "target_count", "在 self 目标规则中必须等于 1", error)
                    || !requireConfigRange(skill.effectRange == 0.0, section, path,
                        "effect_range", "在 self 目标规则中必须等于 0", error)
                    || !requireConfigRange(skill.allowSelf, section, path,
                        "allow_self", "在 self 目标规则中必须是 true", error)))
            {
                return false;
            }

            return true;
        }

        // 此函数读取并校验一个单位节的全部字段及其技能引用。
        bool parseUnitSection(
            const ConfigSection& section,
            const std::filesystem::path& path,
            const std::vector<SkillDefinition>& skills,
            UnitDefinition& unit,
            ConfigError& error)
        {
            if (!rejectUnknownConfigFields(
                    section, path, unitAllowedFields(), "unit", error))
            {
                return false;
            }

            // 此代码段把全部必填单位字段读取到局部对象中。
            std::string basicActionText;
            std::string damageTypeText;
            std::vector<double> levelMultipliers;
            if (!readStringConfigField(section, path, "name", unit.name, error)
                || !readStringListConfigField(section, path, "tags", unit.tags, error)
                || !readIntConfigField(section, path, "max_health", unit.maxHealth, error)
                || !readIntConfigField(section, path, "attack_power", unit.attackPower, error)
                || !readIntConfigField(section, path, "physical_defense", unit.physicalDefense, error)
                || !readIntConfigField(section, path, "magic_resistance", unit.magicResistance, error)
                || !readDoubleConfigField(section, path, "attack_range", unit.attackRange, error)
                || !readDoubleConfigField(section, path, "move_speed", unit.moveSpeed, error)
                || !readIntConfigField(section, path, "attack_speed", unit.attackSpeed, error)
                || !readIntConfigField(section, path, "guard_damage", unit.guardDamage, error)
                || !readIntConfigField(section, path, "price", unit.price, error)
                || !readIntConfigField(section, path, "initial_mana", unit.initialMana, error)
                || !readIntConfigField(section, path, "max_mana", unit.maxMana, error)
                || !readStringConfigField(section, path, "basic_action", basicActionText, error)
                || !readStringConfigField(section, path, "basic_damage_type", damageTypeText, error)
                || !readStringConfigField(section, path, "skill_id", unit.skillId, error)
                || !readDoubleListConfigField(section, path,
                    "level_multipliers", levelMultipliers, error))
            {
                return false;
            }

            // 此代码段把单位文本枚举严格转换为定义中的枚举类型。
            if (!parseBasicAction(basicActionText, unit.basicAction))
            {
                return failInvalidEnum(section, path, "basic_action", error);
            }
            if (!parseDamageType(damageTypeText, unit.basicDamageType))
            {
                return failInvalidEnum(section, path, "basic_damage_type", error);
            }

            // 此代码段验证标签均为合法且不重复的配置 ID。
            std::set<std::string> uniqueTags;
            for (const std::string& tag : unit.tags)
            {
                if (!isConfigIdentifier(tag) || !uniqueTags.insert(tag).second)
                {
                    return failConfig(
                        error,
                        ConfigErrorCategory::TypeError,
                        path,
                        section.fields.at("tags").line,
                        "字段 tags 必须包含不重复的合法 ID");
                }
            }

            // 此代码段校验所有单位数值字段的冻结范围。
            if (!requireConfigRange(unit.maxHealth > 0, section, path,
                    "max_health", "必须大于 0", error)
                || !requireConfigRange(unit.attackPower >= 0, section, path,
                    "attack_power", "必须大于或等于 0", error)
                || !requireConfigRange(unit.physicalDefense >= 0, section, path,
                    "physical_defense", "必须大于或等于 0", error)
                || !requireConfigRange(
                    unit.magicResistance >= 0 && unit.magicResistance <= 100,
                    section, path, "magic_resistance", "必须位于 0 到 100 之间", error)
                || !requireConfigRange(unit.attackRange > 0.0, section, path,
                    "attack_range", "必须大于 0", error)
                || !requireConfigRange(unit.moveSpeed > 0.0, section, path,
                    "move_speed", "必须大于 0", error)
                || !requireConfigRange(
                    unit.attackSpeed >= 0 && unit.attackSpeed <= 600,
                    section, path, "attack_speed", "必须位于 0 到 600 之间", error)
                || !requireConfigRange(unit.guardDamage > 0, section, path,
                    "guard_damage", "必须大于 0", error)
                || !requireConfigRange(unit.price > 0, section, path,
                    "price", "必须大于 0", error)
                || !requireConfigRange(unit.initialMana >= 0, section, path,
                    "initial_mana", "必须大于或等于 0", error)
                || !requireConfigRange(unit.maxMana > 0, section, path,
                    "max_mana", "必须大于 0", error)
                || !requireConfigRange(unit.maxMana >= unit.initialMana, section, path,
                    "max_mana", "不得小于 initial_mana", error))
            {
                return false;
            }

            // 此代码段校验单位等级倍率的数量、范围和单调性。
            if (levelMultipliers.size() != 3)
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::TypeError,
                    path,
                    section.fields.at("level_multipliers").line,
                    "字段 level_multipliers 必须恰好包含三个小数");
            }
            if (!requireConfigRange(
                    std::all_of(levelMultipliers.begin(), levelMultipliers.end(),
                        [](const double value) { return value > 0.0; }),
                    section, path, "level_multipliers",
                    "中的所有值必须大于 0", error)
                || !requireConfigRange(levelMultipliers[0] == 1.0,
                    section, path, "level_multipliers",
                    "的第一个值必须等于 1.0", error)
                || !requireConfigRange(
                    levelMultipliers[0] <= levelMultipliers[1]
                        && levelMultipliers[1] <= levelMultipliers[2],
                    section, path, "level_multipliers", "不得递减", error))
            {
                return false;
            }
            unit.levelMultipliers = {
                levelMultipliers[0], levelMultipliers[1], levelMultipliers[2]};

            // 此代码段校验普通行动与伤害类型之间的条件关系。
            if (unit.basicAction == BasicAction::Attack
                && !requireConfigRange(
                    unit.basicDamageType == DamageType::Physical
                        || unit.basicDamageType == DamageType::Magic,
                    section, path, "basic_damage_type",
                    "在 attack 行动中必须是 physical 或 magic", error))
            {
                return false;
            }
            if (unit.basicAction == BasicAction::Heal
                && (!requireConfigRange(
                        unit.basicDamageType == DamageType::None,
                        section, path, "basic_damage_type",
                        "在 heal 行动中必须是 none", error)
                    || !requireConfigRange(unit.attackPower > 0,
                        section, path, "attack_power",
                        "在 heal 行动中必须大于 0", error)))
            {
                return false;
            }

            // 此代码段验证单位技能 ID 的格式以及对已加载技能的引用。
            if (!isConfigIdentifier(unit.skillId))
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::TypeError,
                    path,
                    section.fields.at("skill_id").line,
                    "字段 skill_id 必须是合法 ID");
            }
            if (!containsSkillId(skills, unit.skillId))
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::ReferenceError,
                    path,
                    section.fields.at("skill_id").line,
                    "字段 skill_id 引用了不存在的技能 " + unit.skillId);
            }

            return true;
        }

        // 此函数读取并校验一个分队节的全部字段。
        bool parseFactionSection(
            const ConfigSection& section,
            const std::filesystem::path& path,
            const GameConfig& gameConfig,
            FactionDefinition& faction,
            ConfigError& error)
        {
            if (!rejectUnknownConfigFields(
                    section, path, factionAllowedFields(), "faction", error))
            {
                return false;
            }

            // 此代码段读取分队的四个必填字段。
            if (!readStringConfigField(section, path, "name", faction.name, error)
                || !readIntConfigField(section, path,
                    "initial_guard", faction.initialGuard, error)
                || !readIntConfigField(section, path,
                    "max_deployed", faction.maxDeployed, error)
                || !readDoubleConfigField(section, path,
                    "price_multiplier", faction.priceMultiplier, error))
            {
                return false;
            }

            // 此代码段校验分队守卫值、部署上限和价格倍率。
            return requireConfigRange(faction.initialGuard > 0, section, path,
                    "initial_guard", "必须大于 0", error)
                && requireConfigRange(faction.maxDeployed > 0, section, path,
                    "max_deployed", "必须大于 0", error)
                && requireConfigRange(
                    faction.maxDeployed <= gameConfig.rosterCapacity,
                    section, path, "max_deployed",
                    "不得超过 roster_capacity", error)
                && requireConfigRange(faction.priceMultiplier > 0.0, section, path,
                    "price_multiplier", "必须大于 0", error);
        }

        // 此函数读取并校验一个分队修正节及其跨定义引用。
        bool parseModifierSection(
            const ConfigSection& section,
            const std::filesystem::path& path,
            const std::vector<FactionDefinition>& factions,
            const std::vector<UnitDefinition>& units,
            FactionModifierDefinition& modifier,
            ConfigError& error)
        {
            if (!rejectUnknownConfigFields(
                    section, path, modifierAllowedFields(), "faction_modifier", error))
            {
                return false;
            }

            // 此代码段读取修正的引用、枚举和值字段。
            std::string attributeText;
            std::string operationText;
            if (!readStringConfigField(section, path,
                    "faction_id", modifier.factionId, error)
                || !readStringConfigField(section, path,
                    "unit_id", modifier.unitId, error)
                || !readStringConfigField(section, path,
                    "attribute", attributeText, error)
                || !readStringConfigField(section, path,
                    "operation", operationText, error)
                || !readDoubleConfigField(section, path, "value", modifier.value, error))
            {
                return false;
            }

            // 此代码段把修正属性与运算文本转换为枚举。
            if (!parseFactionAttribute(attributeText, modifier.attribute))
            {
                return failInvalidEnum(section, path, "attribute", error);
            }
            if (!parseFactionOperation(operationText, modifier.operation))
            {
                return failInvalidEnum(section, path, "operation", error);
            }

            // 此代码段验证修正引用的分队和单位都已加载。
            if (!isConfigIdentifier(modifier.factionId))
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::TypeError,
                    path,
                    section.fields.at("faction_id").line,
                    "字段 faction_id 必须是合法 ID");
            }
            if (!containsFactionId(factions, modifier.factionId))
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::ReferenceError,
                    path,
                    section.fields.at("faction_id").line,
                    "字段 faction_id 引用了不存在的分队 " + modifier.factionId);
            }
            if (modifier.unitId != "*" && !isConfigIdentifier(modifier.unitId))
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::TypeError,
                    path,
                    section.fields.at("unit_id").line,
                    "字段 unit_id 必须是合法 ID 或 *");
            }
            if (modifier.unitId != "*" && !containsUnitId(units, modifier.unitId))
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::ReferenceError,
                    path,
                    section.fields.at("unit_id").line,
                    "字段 unit_id 引用了不存在的单位 " + modifier.unitId);
            }

            // 此代码段限制乘法修正的倍率必须为正数。
            if (modifier.operation == FactionOperation::Multiply
                && !requireConfigRange(modifier.value > 0.0, section, path,
                    "value", "在 multiply 运算中必须大于 0", error))
            {
                return false;
            }

            return true;
        }
    }

    // 此函数加载不依赖其他定义的技能配置。
    bool DefinitionConfigLoader::loadSkills(
        const std::filesystem::path& sourcePath,
        std::vector<SkillDefinition>& skills,
        ConfigError& error)
    {
        error = ConfigError{};

        // 此代码段先完成通用语法解析再处理技能语义。
        ConfigDocument document;
        if (!ConfigParser::parse(sourcePath, document, error))
        {
            return false;
        }

        // 此代码段逐节加载技能并拒绝其他节类型。
        std::vector<SkillDefinition> parsedSkills;
        for (const ConfigSection& section : document.sections)
        {
            if (section.type != "skill" || section.id.empty())
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::UnknownField,
                    sourcePath,
                    section.line,
                    "skills.cfg 只允许带 ID 的 [skill:id] 节");
            }

            SkillDefinition skill;
            skill.id = section.id;
            if (!parseSkillSection(section, sourcePath, skill, error))
            {
                return false;
            }
            parsedSkills.push_back(std::move(skill));
        }

        // 此代码段仅在全部技能成功后提交结果以保持事务性。
        skills = std::move(parsedSkills);
        return true;
    }

    // 此函数加载单位配置并验证每个技能引用。
    bool DefinitionConfigLoader::loadUnits(
        const std::filesystem::path& sourcePath,
        const std::vector<SkillDefinition>& skills,
        std::vector<UnitDefinition>& units,
        ConfigError& error)
    {
        error = ConfigError{};

        // 此代码段先完成通用语法解析再处理单位语义。
        ConfigDocument document;
        if (!ConfigParser::parse(sourcePath, document, error))
        {
            return false;
        }

        // 此代码段逐节加载单位并验证其技能引用。
        std::vector<UnitDefinition> parsedUnits;
        for (const ConfigSection& section : document.sections)
        {
            if (section.type != "unit" || section.id.empty())
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::UnknownField,
                    sourcePath,
                    section.line,
                    "units.cfg 只允许带 ID 的 [unit:id] 节");
            }

            UnitDefinition unit;
            unit.id = section.id;
            if (!parseUnitSection(section, sourcePath, skills, unit, error))
            {
                return false;
            }
            parsedUnits.push_back(std::move(unit));
        }

        // 此代码段仅在全部单位成功后提交结果以保持事务性。
        units = std::move(parsedUnits);
        return true;
    }

    // 此函数加载分队和修正配置并验证全部跨定义引用。
    bool DefinitionConfigLoader::loadFactions(
        const std::filesystem::path& sourcePath,
        const GameConfig& gameConfig,
        const std::vector<UnitDefinition>& units,
        std::vector<FactionDefinition>& factions,
        std::vector<FactionModifierDefinition>& modifiers,
        ConfigError& error)
    {
        error = ConfigError{};

        // 此代码段先完成通用语法解析并拒绝无关节类型。
        ConfigDocument document;
        if (!ConfigParser::parse(sourcePath, document, error))
        {
            return false;
        }
        for (const ConfigSection& section : document.sections)
        {
            if ((section.type != "faction" && section.type != "faction_modifier")
                || section.id.empty())
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::UnknownField,
                    sourcePath,
                    section.line,
                    "factions.cfg 只允许带 ID 的 faction 和 faction_modifier 节");
            }
        }

        // 此代码段第一遍加载所有分队以允许修正节出现在文件任意位置。
        std::vector<FactionDefinition> parsedFactions;
        for (const ConfigSection& section : document.sections)
        {
            if (section.type != "faction")
            {
                continue;
            }

            FactionDefinition faction;
            faction.id = section.id;
            if (!parseFactionSection(
                    section, sourcePath, gameConfig, faction, error))
            {
                return false;
            }
            parsedFactions.push_back(std::move(faction));
        }
        if (parsedFactions.empty())
        {
            return failConfig(
                error,
                ConfigErrorCategory::MissingField,
                sourcePath,
                0,
                "factions.cfg 至少需要一个 [faction:id] 节");
        }

        // 此代码段第二遍按配置顺序加载修正并验证分队与单位引用。
        std::vector<FactionModifierDefinition> parsedModifiers;
        for (const ConfigSection& section : document.sections)
        {
            if (section.type != "faction_modifier")
            {
                continue;
            }

            FactionModifierDefinition modifier;
            modifier.id = section.id;
            if (!parseModifierSection(
                    section,
                    sourcePath,
                    parsedFactions,
                    units,
                    modifier,
                    error))
            {
                return false;
            }
            parsedModifiers.push_back(std::move(modifier));
        }

        // 此代码段仅在全部分队与修正成功后同时提交两个输出集合。
        factions = std::move(parsedFactions);
        modifiers = std::move(parsedModifiers);
        return true;
    }
}
