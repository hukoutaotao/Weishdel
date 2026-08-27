#pragma once

#include "game/animation/SpineAssetRepository.hpp"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>

#include <memory>
#include <string>

namespace autochess::game
{
    enum class UnitAnimationAction
    {
        Relax,
        Move,
        Start,
        Attack,
        Die
    };

    // 此实例为一个场上单位独立维护 Skeleton、AnimationState 和当前动作。
    class UnitAnimationInstance final
    {
    public:
        explicit UnitAnimationInstance(UnitAnimationAssetPtr asset);

        bool play(UnitAnimationAction action, bool force = false);
        void update(float deltaSeconds);
        void draw(
            sf::RenderTarget& target,
            sf::Vector2f position,
            float scale,
            bool faceRight) const;

        bool valid() const noexcept { return asset_ != nullptr; }
        UnitAnimationAction currentAction() const noexcept { return currentAction_; }
        const std::string& currentClip() const noexcept { return currentClip_; }
        bool currentAnimationComplete() const noexcept;
        float scaleForHeight(float targetHeight, bool preparation) const noexcept;

    private:
        UnitAnimationAssetPtr asset_;
        std::unique_ptr<spine::SkeletonDrawable> preparationDrawable_;
        std::unique_ptr<spine::SkeletonDrawable> combatDrawable_;
        spine::SkeletonDrawable* currentDrawable_ = nullptr;
        UnitAnimationAction currentAction_ = UnitAnimationAction::Relax;
        std::string currentClip_;

        static const std::string& clipFor(
            const UnitAnimationSkeletonAsset& asset,
            UnitAnimationAction action);
        static spine::String spineString(const std::string& value);
        bool playClip(
            spine::SkeletonDrawable& drawable,
            const UnitAnimationSkeletonAsset& asset,
            UnitAnimationAction action,
            bool force);
    };
}