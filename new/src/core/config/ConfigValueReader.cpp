#include "core/config/ConfigValueReader.hpp"

#include <charconv>
#include <cctype>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace autochess::core
{
    namespace
    {
        // 此枚举区分成功、格式错误和数值溢出三种转换结果。
        enum class NumberParseResult
        {
            Success,
            InvalidFormat,
            OutOfRange
        };

        // 此函数移除列表元素两端的 ASCII 空白。
        std::string trimListElement(const std::string& text)
        {
            std::size_t first = 0;
            while (first < text.size()
                && std::isspace(static_cast<unsigned char>(text[first])) != 0)
            {
                ++first;
            }

            std::size_t last = text.size();
            while (last > first
                && std::isspace(static_cast<unsigned char>(text[last - 1])) != 0)
            {
                --last;
            }

            return text.substr(first, last - first);
        }

        // 此函数严格解析有符号整数并区分格式错误和数值溢出。
        NumberParseResult parseIntExact(const std::string& text, int& value)
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

        // 此函数检查小数文本只包含可选负号、数字和一个完整小数部分。
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

        // 此函数严格解析有限小数并把异常转换为可验证的结果。
        NumberParseResult parseDoubleExact(const std::string& text, double& value)
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

        // 此函数严格解析 uint32_t 并拒绝负数、杂字符和溢出。
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
    }

    // 此函数统一填写配置错误并返回失败结果。
    bool failConfig(
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

    // 此函数判断文本是否符合配置 ID 的小写 snake_case 规则。
    bool isConfigIdentifier(const std::string& value) noexcept
    {
        if (value.empty() || value.front() < 'a' || value.front() > 'z')
        {
            return false;
        }

        for (const char character : value)
        {
            const bool isLowercase = character >= 'a' && character <= 'z';
            const bool isDigit = character >= '0' && character <= '9';
            if (!isLowercase && !isDigit && character != '_')
            {
                return false;
            }
        }

        return true;
    }

    // 此函数查找必填字段并在缺失时定位到所属节标题。
    const ConfigField* findRequiredConfigField(
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

        failConfig(
            error,
            ConfigErrorCategory::MissingField,
            path,
            section.line,
            "缺少必填字段 " + key);
        return nullptr;
    }

    // 此函数拒绝当前节中所有未在冻结规范声明的字段。
    bool rejectUnknownConfigFields(
        const ConfigSection& section,
        const std::filesystem::path& path,
        const std::set<std::string>& allowedFields,
        const std::string& sectionLabel,
        ConfigError& error)
    {
        for (const auto& field : section.fields)
        {
            if (allowedFields.find(field.first) == allowedFields.end())
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::UnknownField,
                    path,
                    field.second.line,
                    "未知的 " + sectionLabel + " 配置字段 " + field.first);
            }
        }

        return true;
    }

    // 此函数读取一个必填的非空字符串字段。
    bool readStringConfigField(
        const ConfigSection& section,
        const std::filesystem::path& path,
        const std::string& key,
        std::string& value,
        ConfigError& error)
    {
        const ConfigField* const field = findRequiredConfigField(section, key, path, error);
        if (field == nullptr)
        {
            return false;
        }

        value = field->value;
        return true;
    }

    // 此函数严格读取一个必填的有符号整数字段。
    bool readIntConfigField(
        const ConfigSection& section,
        const std::filesystem::path& path,
        const std::string& key,
        int& value,
        ConfigError& error)
    {
        const ConfigField* const field = findRequiredConfigField(section, key, path, error);
        if (field == nullptr)
        {
            return false;
        }

        const NumberParseResult result = parseIntExact(field->value, value);
        if (result == NumberParseResult::Success)
        {
            return true;
        }

        return failConfig(
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

    // 此函数严格读取一个必填的有限小数字段。
    bool readDoubleConfigField(
        const ConfigSection& section,
        const std::filesystem::path& path,
        const std::string& key,
        double& value,
        ConfigError& error)
    {
        const ConfigField* const field = findRequiredConfigField(section, key, path, error);
        if (field == nullptr)
        {
            return false;
        }

        const NumberParseResult result = parseDoubleExact(field->value, value);
        if (result == NumberParseResult::Success)
        {
            return true;
        }

        return failConfig(
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

    // 此函数严格读取一个覆盖完整 uint32_t 范围的字段。
    bool readUint32ConfigField(
        const ConfigSection& section,
        const std::filesystem::path& path,
        const std::string& key,
        std::uint32_t& value,
        ConfigError& error)
    {
        const ConfigField* const field = findRequiredConfigField(section, key, path, error);
        if (field == nullptr)
        {
            return false;
        }

        const NumberParseResult result = parseUint32Exact(field->value, value);
        if (result == NumberParseResult::Success)
        {
            return true;
        }

        return failConfig(
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

    // 此函数把逗号分隔字段读取为不含空元素的字符串列表。
    bool readStringListConfigField(
        const ConfigSection& section,
        const std::filesystem::path& path,
        const std::string& key,
        std::vector<std::string>& values,
        ConfigError& error)
    {
        const ConfigField* const field = findRequiredConfigField(section, key, path, error);
        if (field == nullptr)
        {
            return false;
        }

        std::vector<std::string> parsedValues;
        std::size_t start = 0;
        while (start <= field->value.size())
        {
            const std::size_t separator = field->value.find(',', start);
            const std::size_t end = separator == std::string::npos
                ? field->value.size()
                : separator;
            const std::string element = trimListElement(
                field->value.substr(start, end - start));
            if (element.empty())
            {
                return failConfig(
                    error,
                    ConfigErrorCategory::TypeError,
                    path,
                    field->line,
                    "字段 " + key + " 的列表不得包含空元素");
            }

            parsedValues.push_back(element);
            if (separator == std::string::npos)
            {
                break;
            }
            start = separator + 1;
        }

        values = std::move(parsedValues);
        return true;
    }

    // 此函数把逗号分隔字段严格读取为有限小数列表。
    bool readDoubleListConfigField(
        const ConfigSection& section,
        const std::filesystem::path& path,
        const std::string& key,
        std::vector<double>& values,
        ConfigError& error)
    {
        std::vector<std::string> texts;
        if (!readStringListConfigField(section, path, key, texts, error))
        {
            return false;
        }

        std::vector<double> parsedValues;
        parsedValues.reserve(texts.size());
        for (const std::string& text : texts)
        {
            double value = 0.0;
            const NumberParseResult result = parseDoubleExact(text, value);
            if (result != NumberParseResult::Success)
            {
                return failConfig(
                    error,
                    result == NumberParseResult::OutOfRange
                        ? ConfigErrorCategory::RangeError
                        : ConfigErrorCategory::TypeError,
                    path,
                    section.fields.at(key).line,
                    result == NumberParseResult::OutOfRange
                        ? "字段 " + key + " 的小数列表包含超出可表示范围的值"
                        : "字段 " + key + " 必须是合法小数列表");
            }
            parsedValues.push_back(value);
        }

        values = std::move(parsedValues);
        return true;
    }

    // 此函数对已读取字段执行范围或条件校验并复用字段行号。
    bool requireConfigRange(
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

        return failConfig(
            error,
            ConfigErrorCategory::RangeError,
            path,
            section.fields.at(key).line,
            "字段 " + key + " " + requirement);
    }
}
