#include "core/units/Unit.hpp"

#include <utility>

namespace autochess::core
{
    // 此代码块保存不可变的单位配置快照供运行时读取。
    Unit::Unit(UnitDefinition definition)
        : definition_(std::move(definition))
    {
    }

    const UnitDefinition& Unit::definition() const noexcept
    {
        return definition_;
    }

    // 此代码块实现主动技能单位共享的技能 ID 和释放前置条件。
    ActiveSkillUnit::ActiveSkillUnit(UnitDefinition definition)
        : Unit(std::move(definition))
    {
    }

    const std::string& ActiveSkillUnit::skillId() const noexcept
    {
        return definition().skillId;
    }

    bool ActiveSkillUnit::canReleaseSkill(
        const int currentMana,
        const int maxMana,
        const bool skillActive) const noexcept
    {
        return maxMana > 0 && currentMana >= maxMana && !skillActive;
    }

    // 此代码块将每个具体单位映射到唯一课程角色而不硬编码数值。
    IronGuardUnit::IronGuardUnit(UnitDefinition definition)
        : ActiveSkillUnit(std::move(definition))
    {
    }

    UnitRole IronGuardUnit::role() const noexcept
    {
        return UnitRole::Defender;
    }

    DuelistUnit::DuelistUnit(UnitDefinition definition)
        : ActiveSkillUnit(std::move(definition))
    {
    }

    UnitRole DuelistUnit::role() const noexcept
    {
        return UnitRole::MeleeDamage;
    }

    RangerUnit::RangerUnit(UnitDefinition definition)
        : ActiveSkillUnit(std::move(definition))
    {
    }

    UnitRole RangerUnit::role() const noexcept
    {
        return UnitRole::RangedPhysical;
    }

    ArcanistUnit::ArcanistUnit(UnitDefinition definition)
        : ActiveSkillUnit(std::move(definition))
    {
    }

    UnitRole ArcanistUnit::role() const noexcept
    {
        return UnitRole::MultiTargetMagic;
    }

    MedicUnit::MedicUnit(UnitDefinition definition)
        : ActiveSkillUnit(std::move(definition))
    {
    }

    UnitRole MedicUnit::role() const noexcept
    {
        return UnitRole::Healer;
    }
}
