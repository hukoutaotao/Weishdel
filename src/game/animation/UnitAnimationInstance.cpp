#include "game/animation/UnitAnimationInstance.hpp"

#include <spine/Animation.h>
#include <spine/AnimationState.h>
#include <spine/Bone.h>
#include <spine/MixBlend.h>
#include <spine/MixDirection.h>
#include <spine/Skeleton.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace autochess::game
{
    namespace
    {
        constexpr int AnimationBoundsSampleCount = 16;

        bool validBounds(
            const float x,
            const float y,
            const float width,
            const float height) noexcept
        {
            return std::isfinite(x)
                && std::isfinite(y)
                && std::isfinite(width)
                && std::isfinite(height)
                && width > 0.001F
                && height > 0.001F;
        }
    }

    UnitAnimationInstance::UnitAnimationInstance(UnitAnimationAssetPtr asset)
        : asset_(std::move(asset))
    {
        if (asset_ == nullptr)
        {
            return;
        }
        preparationMetrics_ = calculateVisualMetrics(
            asset_->preparation,
            {
                asset_->preparation.manifest.relax,
                asset_->preparation.manifest.move
            });
        combatMetrics_ = calculateVisualMetrics(
            asset_->combat,
            {
                asset_->combat.manifest.attack,
                asset_->combat.manifest.start,
                asset_->combat.manifest.attackBegin,
                asset_->combat.manifest.attackEnd,
                asset_->combat.manifest.die
            });
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

    UnitAnimationInstance::VisualMetrics
    UnitAnimationInstance::calculateVisualMetrics(
        const UnitAnimationSkeletonAsset& asset,
        const std::vector<std::string>& preferredClips)
    {
        VisualMetrics metrics;
        if (asset.skeletonData == nullptr)
        {
            return metrics;
        }

        // spine-sfml 全局使用向下为正的 Y 轴；测量必须采用同一坐标系。
        spine::Bone::setYDown(true);
        spine::Skeleton skeleton(asset.skeletonData.get());
        spine::Vector<float> vertexBuffer;
        skeleton.setPosition(0.0F, 0.0F);
        skeleton.setScaleX(1.0F);
        skeleton.setScaleY(1.0F);

        const auto samplePose = [&](spine::Animation* animation, const float time)
        {
            skeleton.setToSetupPose();
            if (animation != nullptr)
            {
                animation->apply(
                    skeleton,
                    0.0F,
                    time,
                    false,
                    nullptr,
                    1.0F,
                    spine::MixBlend_Replace,
                    spine::MixDirection_In);
            }
            skeleton.updateWorldTransform();

            float x = 0.0F;
            float y = 0.0F;
            float width = 0.0F;
            float height = 0.0F;
            skeleton.getBounds(x, y, width, height, vertexBuffer);
            if (!validBounds(x, y, width, height))
            {
                return;
            }

            // 锚点只取首个可靠姿态，后续动作不会让角色整体在格子中抖动；
            // 缩放高度则取所有采样姿态的最大值，避免切换 skeleton 后忽大忽小。
            if (!metrics.valid)
            {
                metrics.anchorX = x + width * 0.5F;
                metrics.anchorY = y + height * 0.5F;
                metrics.valid = true;
            }
            metrics.referenceHeight = std::max(metrics.referenceHeight, height);
        };

        bool sampledAnimation = false;
        for (const std::string& clip : preferredClips)
        {
            if (clip.empty())
            {
                continue;
            }
            spine::Animation* animation = asset.skeletonData->findAnimation(
                spineString(clip));
            if (animation == nullptr)
            {
                continue;
            }
            sampledAnimation = true;
            const float duration = std::max(0.0F, animation->getDuration());
            for (int sample = 0; sample <= AnimationBoundsSampleCount; ++sample)
            {
                const float time = duration
                    * static_cast<float>(sample)
                    / static_cast<float>(AnimationBoundsSampleCount);
                samplePose(animation, time);
            }
        }

        if (!sampledAnimation || !metrics.valid)
        {
            samplePose(nullptr, 0.0F);
        }
        return metrics;
    }

    const UnitAnimationInstance::VisualMetrics&
    UnitAnimationInstance::currentVisualMetrics() const noexcept
    {
        return currentDrawable_ == preparationDrawable_.get()
            ? preparationMetrics_
            : combatMetrics_;
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
        const float targetHeight) const noexcept
    {
        if (asset_ == nullptr || currentDrawable_ == nullptr)
        {
            return 1.0F;
        }

        const VisualMetrics& metrics = currentVisualMetrics();
        if (!metrics.valid
            || metrics.referenceHeight <= 0.0F
            || targetHeight <= 0.0F)
        {
            // 无法取得可见附件边界时使用保守比例，避免异常素材撑满窗口。
            return 0.01F;
        }
        return std::clamp(
            targetHeight / metrics.referenceHeight,
            0.001F,
            4.0F);
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
        const float horizontalScale = (faceRight ? 1.0F : -1.0F) * safeScale;
        const VisualMetrics& metrics = currentVisualMetrics();
        const float anchorX = metrics.valid ? metrics.anchorX : 0.0F;
        const float anchorY = metrics.valid ? metrics.anchorY : 0.0F;

        // 使用固定的可见姿态中心作锚点，而不是假定各素材的 skeleton 原点
        // 都位于角色中心。翻转时水平锚点也必须同步翻转。
        currentDrawable_->skeleton->setScaleX(horizontalScale);
        currentDrawable_->skeleton->setScaleY(safeScale);
        currentDrawable_->skeleton->setPosition(
            position.x - anchorX * horizontalScale,
            position.y - anchorY * safeScale);

        // SkeletonDrawable::draw 不会刷新骨骼世界变换，绘制前必须更新，
        // 否则位置和缩放要到下一帧才生效，首次显示时可能仍在窗口外。
        currentDrawable_->skeleton->updateWorldTransform();
        target.draw(*currentDrawable_);
    }
}
