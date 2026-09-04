#pragma once

#include "core/config/ConfigParser.hpp"

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace autochess::core
{
    // 此函数统一填写配置错误并返回失败结果。
    bool failConfig(
        ConfigError& error,
        ConfigErrorCategory category,
        const std::filesystem::path& path,
        std::size_t line,
        const std::string& message);

    // 此函数判断文本是否符合配置 ID 的小写 snake_case 规则。
    bool isConfigIdentifier(const std::string& value) noexcept;

    // 此函数查找必填字段并在缺失时定位到所属节标题。
    const ConfigField* findRequiredConfigField(
        const ConfigSection& section,
        const std::string& key,
        const std::filesystem::path& path,
        ConfigError& error);

    // 此函数拒绝当前节中所有未在冻结规范声明的字段。
    bool rejectUnknownConfigFields(
        const ConfigSection& section,
        const std::filesystem::path& path,
        const std::set<std::string>& allowedFields,
        const std::string& sectionLabel,
        ConfigError& error);

    // 此函数读取一个必填的非空字符串字段。
    bool readStringConfigField(
        const ConfigSection& section,
        const std::filesystem::path& path,
        const std::string& key,
        std::string& value,
        ConfigError& error);

    // 此函数严格读取一个必填的有符号整数字段。
    bool readIntConfigField(
        const ConfigSection& section,
        const std::filesystem::path& path,
        const std::string& key,
        int& value,
        ConfigError& error);

    // 此函数严格读取一个必填的有限小数字段。
    bool readDoubleConfigField(
        const ConfigSection& section,
        const std::filesystem::path& path,
        const std::string& key,
        double& value,
        ConfigError& error);

    // 此函数严格读取一个覆盖完整 uint32_t 范围的字段。
    bool readUint32ConfigField(
        const ConfigSection& section,
        const std::filesystem::path& path,
        const std::string& key,
        std::uint32_t& value,
        ConfigError& error);

    // 此函数把逗号分隔字段读取为不含空元素的字符串列表。
    bool readStringListConfigField(
        const ConfigSection& section,
        const std::filesystem::path& path,
        const std::string& key,
        std::vector<std::string>& values,
        ConfigError& error);

    // 此函数把逗号分隔字段严格读取为有限小数列表。
    bool readDoubleListConfigField(
        const ConfigSection& section,
        const std::filesystem::path& path,
        const std::string& key,
        std::vector<double>& values,
        ConfigError& error);

    // 此函数对已读取字段执行范围或条件校验并复用字段行号。
    bool requireConfigRange(
        bool condition,
        const ConfigSection& section,
        const std::filesystem::path& path,
        const std::string& key,
        const std::string& requirement,
        ConfigError& error);
}
