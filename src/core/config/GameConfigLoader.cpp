#include "core/config/GameConfigLoader.hpp"

#include "core/config/ConfigParser.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>

namespace autochess::core
{
    namespace
    {
        enum class NumberParseResult
        {
            Success,
            InvalidFormat,
            OutOfRange
        };

        // 此辅助函数统一填写配置错误并返回失败结果。
        bool fail(
            ConfigError& error,
            const ConfigErrorCategory category,
            const std::filesystem::path& path,
            const std::size_t line,
            const std::string& message)
        {
            error.category = category;
            error.sourcePath = path;
            error.line = line;
            error.message = message;
            return false;
        }

        // 此辅助函数查找必填字段并在缺失时定位到所属节标题。
        const ConfigField* findRequiredField(
            const ConfigSection& section,
            const std::string& key,
            const std::filesystem::path& path,
            ConfigError& error)
        {
            const auto field = section.fields.find(key);
            if (field != section.fields.end())
            {
                return &field->second;
            }

            fail(
                error,
                ConfigErrorCategory::MissingField,
                path,
                section.line,
                "缺少必填字段 " + key);
            return nullptr;
        }

        // 此辅助函数严格解析有符号整数并区分格式错误和数值溢出。
        NumberParseResult parseIntExact(
            const std::string& text,
            int& value)
        {
            if (text.empty() || text.front() == '+')
            {
                return NumberParseResult::InvalidFormat;
            }

            int parsedValue = 0;
            const char* const first = text.data();
            const char* const last = first + text.size();
            const auto result = std::from_chars(first, last, parsedValue);
            if (result.ec == std::errc::result_out_of_range)
            {
                return NumberParseResult::OutOfRange;
            }
            if (result.ec != std::errc{} || result.ptr != last)
            {
                return NumberParseResult::InvalidFormat;
            }

            value = parsedValue;
            return NumberParseResult::Success;
        }

        // 此辅助函数检查小数文本只包含可选负号、数字和一个完整小数部分。
        bool hasStrictDecimalFormat(const std::string& text)
        {
            if (text.empty() || text.front() == '+')
            {
                return false;
            }

            std::size_t index = text.front() == '-' ? 1U : 0U;
            if (index == text.size())
            {
                return false;
            }

            const std::size_t integerStart = index;
            while (index < text.size() && text[index] >= '0' && text[index] <= '9')
            {
                ++index;
            }
            if (index == integerStart)
            {
                return false;
            }

            if (index == text.size())
            {
                return true;
            }
            if (text[index] != '.')
            {
                return false;
            }

            ++index;
            const std::size_t fractionStart = index;
            while (index < text.size() && text[index] >= '0' && text[index] <= '9')
            {
                ++index;
            }

            return index == text.size() && index > fractionStart;
        }

        // 此辅助函数严格解析有限小数并把异常转换为可验证的结果。
        NumberParseResult parseDoubleExact(
            const std::string& text,
            double& value)
        {
            if (!hasStrictDecimalFormat(text))
            {
                return NumberParseResult::InvalidFormat;
            }

            try
            {
                std::size_t consumed = 0;
                const double parsedValue = std::stod(text, &consumed);
                if (consumed != text.size())
                {
                    return NumberParseResult::InvalidFormat;
                }
                if (!std::isfinite(parsedValue))
                {
                    return NumberParseResult::OutOfRange;
                }

                value = parsedValue;
                return NumberParseResult::Success;
            }
            catch (const std::invalid_argument&)
            {
                return NumberParseResult::InvalidFormat;
            }
            catch (const std::out_of_range&)
            {
                return NumberParseResult::OutOfRange;
            }
        }

        // 此辅助函数严格解析 uint32_t 随机种子并拒绝负数、杂字符和溢出。
        NumberParseResult parseUint32Exact(
            const std::string& text,
            std::uint32_t& value)
        {
            if (text.empty() || text.front() == '+' || text.front() == '-')
            {
                return NumberParseResult::InvalidFormat;
            }

            std::uint64_t parsedValue = 0;
            const char* const first = text.data();
            const char* const last = first + text.size();
            const auto result = std::from_chars(first, last, parsedValue);
            if (result.ec == std::errc::result_out_of_range
                || parsedValue > std::numeric_limits<std::uint32_t>::max())
            {
                return NumberParseResult::OutOfRange;
            }
            if (result.ec != std::errc{} || result.ptr != last)
            {
                return NumberParseResult::InvalidFormat;
            }

            value = static_cast<std::uint32_t>(parsedValue);
            return NumberParseResult::Success;
        }

        // 此辅助函数读取必填整数字段并生成带字段行号的类型错误。
        bool readIntField(
            const ConfigSection& section,
            const std::filesystem::path& path,
            const std::string& key,
            int& value,
            ConfigError& error)
        {
            const ConfigField* const field = findRequiredField(section, key, path, error);
            if (field == nullptr)
            {
                return false;
            }

            const NumberParseResult result = parseIntExact(field->value, value);
            if (result == NumberParseResult::Success)
            {
                return true;
            }

            return fail(
                error,
                result == NumberParseResult::OutOfRange
                    ? ConfigErrorCategory::RangeError
                    : ConfigErrorCategory::TypeError,
                path,
                field->line,
                result == NumberParseResult::OutOfRange
                    ? "字段 " + key + " 的整数超出可表示范围"
                    : "字段 " + key + " 必须是合法整数");
        }

        // 此辅助函数读取必填小数字段并生成带字段行号的转换错误。
        bool readDoubleField(
            const ConfigSection& section,
            const std::filesystem::path& path,
            const std::string& key,
            double& value,
            ConfigError& error)
        {
            const ConfigField* const field = findRequiredField(section, key, path, error);
            if (field == nullptr)
            {
                return false;
            }

            const NumberParseResult result = parseDoubleExact(field->value, value);
            if (result == NumberParseResult::Success)
            {
                return true;
            }

            return fail(
                error,
                result == NumberParseResult::OutOfRange
                    ? ConfigErrorCategory::RangeError
                    : ConfigErrorCategory::TypeError,
                path,
                field->line,
                result == NumberParseResult::OutOfRange
                    ? "字段 " + key + " 的小数超出可表示范围"
                    : "字段 " + key + " 必须是合法小数");
        }

        // 此辅助函数读取必填随机种子并保留 uint32_t 的完整合法范围。
        bool readUint32Field(
            const ConfigSection& section,
            const std::filesystem::path& path,
            const std::string& key,
            std::uint32_t& value,
            ConfigError& error)
        {
            const ConfigField* const field = findRequiredField(section, key, path, error);
            if (field == nullptr)
            {
                return false;
            }

            const NumberParseResult result = parseUint32Exact(field->value, value);
            if (result == NumberParseResult::Success)
            {
                return true;
            }

            return fail(
                error,
                result == NumberParseResult::OutOfRange
                    ? ConfigErrorCategory::RangeError
                    : ConfigErrorCategory::TypeError,
                path,
                field->line,
                result == NumberParseResult::OutOfRange
                    ? "字段 " + key + " 必须位于 0 到 4294967295 之间"
                    : "字段 " + key + " 必须是无符号整数");
        }

        // 此辅助函数对已转换字段执行单项范围校验并复用原字段行号。
        bool requireRange(
            const bool condition,
            const ConfigSection& section,
            const std::filesystem::path& path,
            const std::string& key,
            const std::string& requirement,
            ConfigError& error)
        {
            if (condition)
            {
                return true;
            }

            return fail(
                error,
                ConfigErrorCategory::RangeError,
                path,
                section.fields.at(key).line,
                "字段 " + key + " " + requirement);
        }
    }

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
                return fail(
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
            return fail(
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
        for (const auto& field : gameSection->fields)
        {
            if (allowedFields.find(field.first) == allowedFields.end())
            {
                return fail(
                    error,
                    ConfigErrorCategory::UnknownField,
                    sourcePath,
                    field.second.line,
                    "未知的 game 配置字段 " + field.first);
            }
        }

        // 将全部必填文本字段严格转换到临时对象以避免失败时污染输出配置。
        GameConfig parsedConfig;
        if (!readIntField(*gameSection, sourcePath, "max_rounds", parsedConfig.maxRounds, error)
            || !readIntField(*gameSection, sourcePath, "preparation_seconds", parsedConfig.preparationSeconds, error)
            || !readIntField(*gameSection, sourcePath, "combat_timeout_seconds", parsedConfig.combatTimeoutSeconds, error)
            || !readIntField(*gameSection, sourcePath, "starting_gold", parsedConfig.startingGold, error)
            || !readIntField(*gameSection, sourcePath, "round_income", parsedConfig.roundIncome, error)
            || !readIntField(*gameSection, sourcePath, "loser_bonus", parsedConfig.loserBonus, error)
            || !readIntField(*gameSection, sourcePath, "shop_slots", parsedConfig.shopSlots, error)
            || !readIntField(*gameSection, sourcePath, "shop_refresh_cost", parsedConfig.shopRefreshCost, error)
            || !readIntField(*gameSection, sourcePath, "roster_capacity", parsedConfig.rosterCapacity, error)
            || !readDoubleField(*gameSection, sourcePath, "sell_ratio", parsedConfig.sellRatio, error)
            || !readDoubleField(*gameSection, sourcePath, "merge_refund_ratio", parsedConfig.mergeRefundRatio, error)
            || !readDoubleField(*gameSection, sourcePath, "revive_ratio", parsedConfig.reviveRatio, error)
            || !readIntField(*gameSection, sourcePath, "max_unit_level", parsedConfig.maxUnitLevel, error)
            || !readUint32Field(*gameSection, sourcePath, "random_seed", parsedConfig.randomSeed, error))
        {
            return false;
        }

        // 校验必须为正数的回合、计时和容量字段。
        if (!requireRange(parsedConfig.maxRounds > 0, *gameSection, sourcePath,
                "max_rounds", "必须大于 0", error)
            || !requireRange(parsedConfig.preparationSeconds > 0, *gameSection, sourcePath,
                "preparation_seconds", "必须大于 0", error)
            || !requireRange(parsedConfig.combatTimeoutSeconds > 0, *gameSection, sourcePath,
                "combat_timeout_seconds", "必须大于 0", error)
            || !requireRange(parsedConfig.rosterCapacity > 0, *gameSection, sourcePath,
                "roster_capacity", "必须大于 0", error))
        {
            return false;
        }

        // 校验金币与费用字段均不得为负数。
        if (!requireRange(parsedConfig.startingGold >= 0, *gameSection, sourcePath,
                "starting_gold", "必须大于或等于 0", error)
            || !requireRange(parsedConfig.roundIncome >= 0, *gameSection, sourcePath,
                "round_income", "必须大于或等于 0", error)
            || !requireRange(parsedConfig.loserBonus >= 0, *gameSection, sourcePath,
                "loser_bonus", "必须大于或等于 0", error)
            || !requireRange(parsedConfig.shopRefreshCost >= 0, *gameSection, sourcePath,
                "shop_refresh_cost", "必须大于或等于 0", error))
        {
            return false;
        }

        // 校验由课程规则固定的商店槽位数和单位最高等级。
        if (!requireRange(parsedConfig.shopSlots == 6, *gameSection, sourcePath,
                "shop_slots", "必须等于 6", error)
            || !requireRange(parsedConfig.maxUnitLevel == 3, *gameSection, sourcePath,
                "max_unit_level", "必须等于 3", error))
        {
            return false;
        }

        // 校验三个经济比例都位于闭区间 0 到 1 内。
        if (!requireRange(parsedConfig.sellRatio >= 0.0 && parsedConfig.sellRatio <= 1.0,
                *gameSection, sourcePath, "sell_ratio", "必须位于 0.0 到 1.0 之间", error)
            || !requireRange(parsedConfig.mergeRefundRatio >= 0.0 && parsedConfig.mergeRefundRatio <= 1.0,
                *gameSection, sourcePath, "merge_refund_ratio", "必须位于 0.0 到 1.0 之间", error)
            || !requireRange(parsedConfig.reviveRatio >= 0.0 && parsedConfig.reviveRatio <= 1.0,
                *gameSection, sourcePath, "revive_ratio", "必须位于 0.0 到 1.0 之间", error))
        {
            return false;
        }

        // 只有全部校验成功后才提交临时配置，保证失败调用不改变输出对象。
        config = parsedConfig;
        return true;
    }
}
