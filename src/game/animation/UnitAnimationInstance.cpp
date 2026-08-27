#include "game/animation/UnitAnimationInstance.hpp"

#include <spine/Animation.h>
#include <spine/AnimationState.h>
#include <spine/Skeleton.h>

#include <algorithm>
#include <utility>

namespace autochess::game
{
    UnitAnimationInstance::UnitAnimationInstance(UnitAnimationAssetPtr asset)
        : asset_(std::move(asset))
    {
        if (asset_ == nullptr)
        {
            return;
        }
        preparationDrawable_ = std::make_unique<spine::SkeletonDrawable>(
            asset_->preparation.skeletonData.get());
        combatDrawable_ = std::make_unique<spine::SkeletonDrawable>(
            asset_->combat.skeletonData.get());
        currentDrawable_ = preparationDrawable_.get();
        play(UnitAnimationAction::Relax, true);
    }

    const std::string& UnitAnimationInstance::clipFor(
        const UnitAnimationSkeletonAsset& asset,
        const UnitAnimationAction action)
    {
        static const std::string empty;
        switch (action)
        {
        case UnitAnimationAction::Relax:
            return asset.manifest.relax;
        case UnitAnimationAction::Move:
            return asset.manifest.move.empty()
                ? asset.manifest.relax
                : asset.manifest.move;
        case UnitAnimationAction::Start:
            return asset.manifest.start;
        case UnitAnimationAction::Attack:
            return asset.manifest.attack;
        case UnitAnimationAction::Die:
            if (!asset.manifest.die.empty())
            {
                return asset.manifest.die;
            }
            if (!asset.manifest.start.empty())
            {
                return asset.manifest.start;
            }
            return asset.manifest.relax;
        }
        return empty;
    }

    spine::String UnitAnimationInstance::spineString(
        const std::string& value)
    {
        return spine::String(value.c_str());
    }

    bool UnitAnimationInstance::play(
        const UnitAnimationAction action,
        const bool force)
    {
        if (asset_ == nullptr)
        {
            return false;
        }
        const bool preparationAction =
            action == UnitAnimationAction::Relax
            || action == UnitAnimationAction::Move;
        spine::SkeletonDrawable* drawable = preparationAction
            ? preparationDrawable_.get()
            : combatDrawable_.get();
        const UnitAnimationSkeletonAsset& skeletonAsset = preparationAction
            ? asset_->preparation
            : asset_->combat;
        if (drawable == nullptr || skeletonAsset.skeletonData == nullptr)
        {
            return false;
        }

        const std::string& clip = clipFor(skeletonAsset, action);
        if (clip.empty())
        {
            return false;
        }
        if (!force && currentDrawable_ == drawable && currentAction_ == action)
        {
            return true;
        }
        return playClip(*drawable, skeletonAsset, action, force);
    }

    bool UnitAnimationInstance::playClip(
        spine::SkeletonDrawable& drawable,
        const UnitAnimationSkeletonAsset& asset,
        const UnitAnimationAction action,
        const bool force)
    {
        const std::string& clip = clipFor(asset, action);
        if (clip.empty() || (!force && currentClip_ == clip
                             && currentDrawable_ == &drawable))
        {
            return !clip.empty();
        }

        drawable.skeleton->setToSetupPose();
        if (action == UnitAnimationAction::Attack
            && asset.manifest.hasAttackSequence())
        {
            drawable.state->setAnimation(
                0,
                spineString(asset.manifest.attackBegin),
                false);
            drawable.state->addAnimation(
                0,
                spineString(asset.manifest.attack),
                false,
                0.0F);
            drawable.state->addAnimation(
                0,
                spineString(asset.manifest.attackEnd),
                false,
                0.0F);
        }
        else
        {
            const bool loop = action == UnitAnimationAction::Relax
                || action == UnitAnimationAction::Move;
            drawable.state->setAnimation(0, spineString(clip), loop);
        }
        drawable.update(0.0F);
        currentDrawable_ = &drawable;
        currentAction_ = action;
        currentClip_ = clip;
        return true;
    }

    bool UnitAnimationInstance::currentAnimationComplete() const noexcept
    {
        if (currentDrawable_ == nullptr || currentDrawable_->state == nullptr)
        {
            return true;
        }
        spine::TrackEntry* entry = currentDrawable_->state->getCurrent(0);
        return entry == nullptr || entry->isComplete();
    }

    float UnitAnimationInstance::scaleForHeight(
        const float targetHeight, const bool preparation) const noexcept
    {
        if (asset_ == nullptr)
        {
            return 1.0F;
        }
        const auto& skeletonAsset = preparation
            ? asset_->preparation
            : asset_->combat;
        const float height = skeletonAsset.skeletonData == nullptr
            ? 0.0F
            : skeletonAsset.skeletonData->getHeight();
        return height > 0.0F
            ? std::max(0.001F, targetHeight / height)
            : 1.0F;
    }

    void UnitAnimationInstance::update(const float deltaSeconds)
    {
        if (currentDrawable_ != nullptr)
        {
            currentDrawable_->update(std::max(0.0F, deltaSeconds));
        }
    }

    void UnitAnimationInstance::draw(
        sf::RenderTarget& target,
        const sf::Vector2f position,
        const float scale,
        const bool faceRight) const
    {
        if (currentDrawable_ == nullptr || currentDrawable_->skeleton == nullptr)
        {
            return;
        }
        const float safeScale = std::max(0.001F, scale);
        currentDrawable_->skeleton->setPosition(position.x, position.y);
        currentDrawable_->skeleton->setScaleX(
            (faceRight ? 1.0F : -1.0F) * safeScale);
        currentDrawable_->skeleton->setScaleY(safeScale);
        target.draw(*currentDrawable_);
    }
}