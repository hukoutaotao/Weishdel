#include "core/config/GameConfigLoader.hpp"

#include "core/config/ConfigParser.hpp"
#include "core/config/ConfigValueReader.hpp"

#include <set>
#include <string>

namespace autochess::core
{
    // 此函数按固定顺序完成语法解析、结构检查、类型转换和范围校验。
    bool GameConfigLoader::load(
        const std::filesystem::path& sourcePath,
        GameConfig& config,
        ConfigError& error)
    {
        error = ConfigError{};

        // 先调用通用解析器，确保后续逻辑只处理结构化节和字段。
        ConfigDocument document;
        if (!ConfigParser::parse(sourcePath, document, error))
        {
            return false;
        }

        // 检查文件只包含一个不带 ID 的 game 节。
        const ConfigSection* gameSection = nullptr;
        for (const ConfigSection& section : document.sections)
        {
            if (section.type != "game" || !section.id.empty())
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::UnknownField,
                    sourcePath,
                    section.line,
                    "game.cfg 只允许不带 ID 的 [game] 节");
            }
            gameSection = &section;
        }
        if (gameSection == nullptr)
        {
            return failConfig(
                error,
                ConfigErrorCategory::MissingField,
                sourcePath,
                0,
                "缺少 [game] 节");
        }

        // 拒绝所有未在冻结规范中声明的游戏配置字段。
        const std::set<std::string> allowedFields = {
            "max_rounds",
            "preparation_seconds",
            "combat_timeout_seconds",
            "starting_gold",
            "round_income",
            "loser_bonus",
            "shop_slots",
            "shop_refresh_cost",
            "roster_capacity",
            "sell_ratio",
            "merge_refund_ratio",
            "revive_ratio",
            "max_unit_level",
            "random_seed"};
        if (!rejectUnknownConfigFields(
                *gameSection, sourcePath, allowedFields, "game", error))
        {
            return false;
        }

        // 将全部必填文本字段严格转换到临时对象以避免失败时污染输出配置。
        GameConfig parsedConfig;
        if (!readIntConfigField(*gameSection, sourcePath, "max_rounds", parsedConfig.maxRounds, error)
            || !readIntConfigField(*gameSection, sourcePath, "preparation_seconds", parsedConfig.preparationSeconds, error)
            || !readIntConfigField(*gameSection, sourcePath, "combat_timeout_seconds", parsedConfig.combatTimeoutSeconds, error)
            || !readIntConfigField(*gameSection, sourcePath, "starting_gold", parsedConfig.startingGold, error)
            || !readIntConfigField(*gameSection, sourcePath, "round_income", parsedConfig.roundIncome, error)
            || !readIntConfigField(*gameSection, sourcePath, "loser_bonus", parsedConfig.loserBonus, error)
            || !readIntConfigField(*gameSection, sourcePath, "shop_slots", parsedConfig.shopSlots, error)
            || !readIntConfigField(*gameSection, sourcePath, "shop_refresh_cost", parsedConfig.shopRefreshCost, error)
            || !readIntConfigField(*gameSection, sourcePath, "roster_capacity", parsedConfig.rosterCapacity, error)
            || !readDoubleConfigField(*gameSection, sourcePath, "sell_ratio", parsedConfig.sellRatio, error)
            || !readDoubleConfigField(*gameSection, sourcePath, "merge_refund_ratio", parsedConfig.mergeRefundRatio, error)
            || !readDoubleConfigField(*gameSection, sourcePath, "revive_ratio", parsedConfig.reviveRatio, error)
            || !readIntConfigField(*gameSection, sourcePath, "max_unit_level", parsedConfig.maxUnitLevel, error)
            || !readUint32ConfigField(*gameSection, sourcePath, "random_seed", parsedConfig.randomSeed, error))
        {
            return false;
        }

        // 校验必须为正数的回合、计时和容量字段。
        if (!requireConfigRange(parsedConfig.maxRounds > 0, *gameSection, sourcePath,
                "max_rounds", "必须大于 0", error)
            || !requireConfigRange(parsedConfig.preparationSeconds > 0, *gameSection, sourcePath,
                "preparation_seconds", "必须大于 0", error)
            || !requireConfigRange(parsedConfig.combatTimeoutSeconds > 0, *gameSection, sourcePath,
                "combat_timeout_seconds", "必须大于 0", error)
            || !requireConfigRange(parsedConfig.rosterCapacity > 0, *gameSection, sourcePath,
                "roster_capacity", "必须大于 0", error))
        {
            return false;
        }

        // 校验金币与费用字段均不得为负数。
        if (!requireConfigRange(parsedConfig.startingGold >= 0, *gameSection, sourcePath,
                "starting_gold", "必须大于或等于 0", error)
            || !requireConfigRange(parsedConfig.roundIncome >= 0, *gameSection, sourcePath,
                "round_income", "必须大于或等于 0", error)
            || !requireConfigRange(parsedConfig.loserBonus >= 0, *gameSection, sourcePath,
                "loser_bonus", "必须大于或等于 0", error)
            || !requireConfigRange(parsedConfig.shopRefreshCost >= 0, *gameSection, sourcePath,
                "shop_refresh_cost", "必须大于或等于 0", error))
        {
            return false;
        }

        // 校验由课程规则固定的商店槽位数和单位最高等级。
        if (!requireConfigRange(parsedConfig.shopSlots == 6, *gameSection, sourcePath,
                "shop_slots", "必须等于 6", error)
            || !requireConfigRange(parsedConfig.maxUnitLevel == 3, *gameSection, sourcePath,
                "max_unit_level", "必须等于 3", error))
        {
            return false;
        }

        // 校验三个经济比例都位于闭区间 0 到 1 内。
        if (!requireConfigRange(parsedConfig.sellRatio >= 0.0 && parsedConfig.sellRatio <= 1.0,
                *gameSection, sourcePath, "sell_ratio", "必须位于 0.0 到 1.0 之间", error)
            || !requireConfigRange(parsedConfig.mergeRefundRatio >= 0.0 && parsedConfig.mergeRefundRatio <= 1.0,
                *gameSection, sourcePath, "merge_refund_ratio", "必须位于 0.0 到 1.0 之间", error)
            || !requireConfigRange(parsedConfig.reviveRatio >= 0.0 && parsedConfig.reviveRatio <= 1.0,
                *gameSection, sourcePath, "revive_ratio", "必须位于 0.0 到 1.0 之间", error))
        {
            return false;
        }

        // 只有全部校验成功后才提交临时配置，保证失败调用不改变输出对象。
        config = parsedConfig;
        return true;
    }
}
