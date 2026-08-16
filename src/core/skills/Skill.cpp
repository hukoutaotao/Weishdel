#include "core/skills/Skill.hpp"

#include <cstddef>
#include <utility>

namespace autochess::core
{
    // 此代码块保存技能配置并提供安全的一级到三级数值查询。
    ConfiguredSkill::ConfiguredSkill(SkillDefinition definition)
        : definition_(std::move(definition))
    {
    }

    const SkillDefinition& ConfiguredSkill::definition() const noexcept
    {
        return definition_;
    }

    std::optional<double> ConfiguredSkill::valueForLevel(
        const int level) const noexcept
    {
        if (level < 1 || level > 3)
        {
            return std::nullopt;
        }

        return definition_.levelValues[static_cast<std::size_t>(level - 1)];
    }
}
