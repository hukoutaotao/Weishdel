#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace autochess::core
{
    enum class ConfigErrorCategory
    {
        FileOpen,
        Syntax,
        DuplicateDefinition,
        UnknownField,
        MissingField,
        TypeError,
        RangeError,
        ReferenceError,
        MapValidation
    };

    struct ConfigError
    {
        ConfigErrorCategory category = ConfigErrorCategory::Syntax;
        std::filesystem::path sourcePath;
        std::size_t line = 0;
        std::string message;
    };
}
