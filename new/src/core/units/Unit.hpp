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

    // 以下五个具体类只固定角色，所有数值仍来自配置。
    class IronGuardUnit final : public Unit
    {
    public:
        explicit IronGuardUnit(UnitDefinition definition);
        UnitRole role() const noexcept override;
    };

    class DuelistUnit final : public Unit
    {
    public:
        explicit DuelistUnit(UnitDefinition definition);
        UnitRole role() const noexcept override;
    };

    class RangerUnit final : public Unit
    {
    public:
        explicit RangerUnit(UnitDefinition definition);
        UnitRole role() const noexcept override;
    };

    class ArcanistUnit final : public Unit
    {
    public:
        explicit ArcanistUnit(UnitDefinition definition);
        UnitRole role() const noexcept override;
    };

    class MedicUnit final : public Unit
    {
    public:
        explicit MedicUnit(UnitDefinition definition);
        UnitRole role() const noexcept override;
    };
}
