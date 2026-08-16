#pragma once

#include "core/model/Definitions.hpp"

#include <optional>

namespace autochess::core
{
    // 此抽象基类规定技能运行时必须公开配置和等级数值。
    class Skill
    {
    public:
        virtual ~Skill() = default;

        virtual const SkillDefinition& definition() const noexcept = 0;

        virtual std::optional<double> valueForLevel(
            int level) const noexcept = 0;
    };

    // 此具体类把冻结配置适配为统一技能接口，效果执行将在后续步骤实现。
    class ConfiguredSkill final : public Skill
    {
    public:
        explicit ConfiguredSkill(SkillDefinition definition);

        const SkillDefinition& definition() const noexcept override;

        std::optional<double> valueForLevel(
            int level) const noexcept override;

    private:
        SkillDefinition definition_;
    };
}
