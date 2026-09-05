#include "core/config/ConfigParser.hpp"

#include <cctype>
#include <fstream>
#include <set>
#include <sstream>

namespace autochess::core
{
    namespace
    {
        std::string trim(const std::string& text)
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

        bool isIdentifier(const std::string& value)
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

        bool fail(
            ConfigError& error,
            ConfigErrorCategory category,
            const std::filesystem::path& path,
            std::size_t line,
            const std::string& message)
        {
            error.category = category;
            error.sourcePath = path;
            error.line = line;
            error.message = message;
            return false;
        }
    }

    bool ConfigParser::parse(
        const std::filesystem::path& sourcePath,
        ConfigDocument& document,
        ConfigError& error)
    {
        std::ifstream input(sourcePath, std::ios::binary);
        if (!input)
        {
            return fail(
                error,
                ConfigErrorCategory::FileOpen,
                sourcePath,
                0,
                "无法打开配置文件");
        }

        ConfigDocument parsedDocument;
        parsedDocument.sourcePath = sourcePath;

        std::set<std::string> sectionKeys;
        ConfigSection* currentSection = nullptr;
        std::string rawLine;
        std::size_t lineNumber = 0;

        while (std::getline(input, rawLine))
        {
            ++lineNumber;
            if (lineNumber == 1
                && rawLine.size() >= 3
                && static_cast<unsigned char>(rawLine[0]) == 0xEF
                && static_cast<unsigned char>(rawLine[1]) == 0xBB
                && static_cast<unsigned char>(rawLine[2]) == 0xBF)
            {
                return fail(
                    error,
                    ConfigErrorCategory::Syntax,
                    sourcePath,
                    lineNumber,
                    "配置文件不得包含 UTF-8 BOM");
            }

            const std::string line = trim(rawLine);
            if (line.empty() || line.front() == '#')
            {
                continue;
            }

            if (line.front() == '[')
            {
                if (line.size() < 3 || line.back() != ']')
                {
                    return fail(
                        error,
                        ConfigErrorCategory::Syntax,
                        sourcePath,
                        lineNumber,
                        "节标题缺少右方括号或内容为空");
                }

                const std::string header = line.substr(1, line.size() - 2);
                if (header.find_first_of(" \t[]") != std::string::npos)
                {
                    return fail(
                        error,
                        ConfigErrorCategory::Syntax,
                        sourcePath,
                        lineNumber,
                        "节标题内部不能包含空白或方括号");
                }

                const std::size_t separator = header.find(':');
                if (separator != std::string::npos
                    && header.find(':', separator + 1) != std::string::npos)
                {
                    return fail(
                        error,
                        ConfigErrorCategory::Syntax,
                        sourcePath,
                        lineNumber,
                        "节标题只允许一个冒号");
                }

                ConfigSection section;
                section.type = separator == std::string::npos
                    ? header
                    : header.substr(0, separator);
                section.id = separator == std::string::npos
                    ? std::string{}
                    : header.substr(separator + 1);
                section.line = lineNumber;

                if (!isIdentifier(section.type)
                    || (separator != std::string::npos
                        && !isIdentifier(section.id)))
                {
                    return fail(
                        error,
                        ConfigErrorCategory::Syntax,
                        sourcePath,
                        lineNumber,
                        "节类型或 ID 不符合小写 snake_case 规则");
                }

                const std::string sectionKey = section.type + ':' + section.id;
                if (!sectionKeys.insert(sectionKey).second)
                {
                    return fail(
                        error,
                        ConfigErrorCategory::DuplicateDefinition,
                        sourcePath,
                        lineNumber,
                        "重复定义节 [" + header + "]");
                }

                parsedDocument.sections.push_back(std::move(section));
                currentSection = &parsedDocument.sections.back();
                continue;
            }

            if (currentSection == nullptr)
            {
                return fail(
                    error,
                    ConfigErrorCategory::Syntax,
                    sourcePath,
                    lineNumber,
                    "字段必须写在节标题之后");
            }

            const std::size_t separator = line.find('=');
            if (separator == std::string::npos)
            {
                return fail(
                    error,
                    ConfigErrorCategory::Syntax,
                    sourcePath,
                    lineNumber,
                    "字段缺少等号");
            }

            const std::string key = trim(line.substr(0, separator));
            const std::string value = trim(line.substr(separator + 1));
            if (!isIdentifier(key) || value.empty())
            {
                return fail(
                    error,
                    ConfigErrorCategory::Syntax,
                    sourcePath,
                    lineNumber,
                    "字段名必须是小写 snake_case，且字段值不得为空");
            }

            const auto inserted = currentSection->fields.emplace(
                key,
                ConfigField{value, lineNumber});
            if (!inserted.second)
            {
                return fail(
                    error,
                    ConfigErrorCategory::DuplicateDefinition,
                    sourcePath,
                    lineNumber,
                    "重复定义字段 " + key);
            }
        }

        if (!input.eof())
        {
            return fail(
                error,
                ConfigErrorCategory::FileOpen,
                sourcePath,
                lineNumber,
                "读取配置文件时发生错误");
        }

        if (parsedDocument.sections.empty())
        {
            return fail(
                error,
                ConfigErrorCategory::Syntax,
                sourcePath,
                0,
                "配置文件不包含任何节");
        }

        document = std::move(parsedDocument);
        return true;
    }

    std::string configErrorCategoryName(const ConfigErrorCategory category)
    {
        switch (category)
        {
        case ConfigErrorCategory::FileOpen:
            return "文件打开";
        case ConfigErrorCategory::Syntax:
            return "语法错误";
        case ConfigErrorCategory::DuplicateDefinition:
            return "重复定义";
        case ConfigErrorCategory::UnknownField:
            return "未知字段";
        case ConfigErrorCategory::MissingField:
            return "缺少字段";
        case ConfigErrorCategory::TypeError:
            return "类型错误";
        case ConfigErrorCategory::RangeError:
            return "范围错误";
        case ConfigErrorCategory::ReferenceError:
            return "引用错误";
        case ConfigErrorCategory::MapValidation:
            return "地图校验";
        }

        return "未知错误";
    }

    std::string formatConfigError(const ConfigError& error)
    {
        std::ostringstream output;
        output << error.sourcePath.u8string();
        if (error.line != 0)
        {
            output << ':' << error.line;
        }
        output << ": [" << configErrorCategoryName(error.category) << "] "
               << error.message;
        return output.str();
    }
}
