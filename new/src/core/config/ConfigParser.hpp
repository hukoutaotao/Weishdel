#pragma once

#include "core/config/ConfigError.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace autochess::core
{
    struct ConfigField
    {
        std::string value;
        std::size_t line = 0;
    };

    struct ConfigSection
    {
        std::string type;
        std::string id;
        std::size_t line = 0;
        std::map<std::string, ConfigField> fields;
    };

    struct ConfigDocument
    {
        std::filesystem::path sourcePath;
        std::vector<ConfigSection> sections;
    };

    class ConfigParser
    {
    public:
        static bool parse(
            const std::filesystem::path& sourcePath,
            ConfigDocument& document,
            ConfigError& error);
    };

    std::string configErrorCategoryName(ConfigErrorCategory category);
    std::string formatConfigError(const ConfigError& error);
}
