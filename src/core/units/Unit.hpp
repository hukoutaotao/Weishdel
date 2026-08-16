#pragma once

#include "core/model/Definitions.hpp"

#include <string>

namespace autochess::core
{
    // 此枚举为五个课程要求单位提供稳定且可测试的角色分类。
    enum class UnitRole
    {
        Defender,
        MeleeDamage,
        RangedPhysical,
        MultiTargetMagic,
        Healer
    };

    // 此抽象基类保存配置定义并要求具体单位声明自己的角色。
    class Unit
    {
    public:
        virtual ~Unit() = default;

        const UnitDefinition& definition() const noexcept;

        virtual UnitRole role() const noexcept = 0;

    protected:
        explicit Unit(UnitDefinition definition);

    private:
        UnitDefinition definition_;
    };

    // 此纯接口隔离主动技能使用能力，避免基础单位依赖战斗实现细节。
    class ISkillUser
    {
    public:
        virtual ~ISkillUser() = default;

        virtual const std::string& skillId() const noexcept = 0;

        virtual bool canReleaseSkill(
            int currentMana,
            int maxMana,
            bool skillActive) const noexcept = 0;
    };

    // 此抽象类通过明确的多重继承为五个主动技能单位复用通用规则。
    class ActiveSkillUnit : public Unit, public ISkillUser
    {
    public:
        const std::string& skillId() const noexcept override;

        bool canReleaseSkill(
            int currentMana,
            int maxMana,
            bool skillActive) const noexcept override;

    protected:
        explicit ActiveSkillUnit(UnitDefinition definition);
    };

    // 以下五个具体类只固定角色，所有数值和技能 ID 仍来自配置。
    class IronGuardUnit final : public ActiveSkillUnit
    {
    public:
        explicit IronGuardUnit(UnitDefinition definition);
        UnitRole role() const noexcept override;
    };

    class DuelistUnit final : public ActiveSkillUnit
    {
    public:
        explicit DuelistUnit(UnitDefinition definition);
        UnitRole role() const noexcept override;
    };

    class RangerUnit final : public ActiveSkillUnit
    {
    public:
        explicit RangerUnit(UnitDefinition definition);
        UnitRole role() const noexcept override;
    };

    class ArcanistUnit final : public ActiveSkillUnit
    {
    public:
        explicit ArcanistUnit(UnitDefinition definition);
        UnitRole role() const noexcept override;
    };

    class MedicUnit final : public ActiveSkillUnit
    {
    public:
        explicit MedicUnit(UnitDefinition definition);
        UnitRole role() const noexcept override;
    };
}
