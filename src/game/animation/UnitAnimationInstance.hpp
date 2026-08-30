#pragma once

#include "game/animation/SpineAssetRepository.hpp"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>

#include <memory>
#include <string>
#include <vector>

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
        bool repeatingAttack() const noexcept;
        float scaleForHeight(float targetHeight) const noexcept;

    private:
        struct VisualMetrics
        {
            float anchorX = 0.0F;
            float anchorY = 0.0F;
            float referenceHeight = 0.0F;
            bool valid = false;
        };

        UnitAnimationAssetPtr asset_;
        std::unique_ptr<spine::SkeletonDrawable> preparationDrawable_;
        std::unique_ptr<spine::SkeletonDrawable> combatDrawable_;
        spine::SkeletonDrawable* currentDrawable_ = nullptr;
        VisualMetrics preparationMetrics_;
        VisualMetrics startMetrics_;
        VisualMetrics attackMetrics_;
        VisualMetrics dieMetrics_;
        UnitAnimationAction currentAction_ = UnitAnimationAction::Relax;
        std::string currentClip_;
        bool finishingAttackSequence_ = false;

        static const std::string& clipFor(
            const UnitAnimationSkeletonAsset& asset,
            UnitAnimationAction action);
        static spine::String spineString(const std::string& value);
        static VisualMetrics calculateVisualMetrics(
            const UnitAnimationSkeletonAsset& asset,
            const std::vector<std::string>& preferredClips);
        const VisualMetrics& currentVisualMetrics() const noexcept;
        bool playClip(
            spine::SkeletonDrawable& drawable,
            const UnitAnimationSkeletonAsset& asset,
            UnitAnimationAction action,
            bool force);
    };
}
