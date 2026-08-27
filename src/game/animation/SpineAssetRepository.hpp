#pragma once

#include "game/animation/UnitAnimationManifest.hpp"

#include <spine/Atlas.h>
#include <spine/SkeletonData.h>
#include <spine/spine-sfml.h>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace autochess::game
{
    // 此结构持有一套 atlas、纹理和 skeleton 数据，供多个单位实例共享。
    struct UnitAnimationSkeletonAsset
    {
        std::unique_ptr<spine::SFMLTextureLoader> textureLoader;
        std::unique_ptr<spine::Atlas> atlas;
        std::unique_ptr<spine::SkeletonData> skeletonData;
        UnitAnimationManifest manifest;

        bool valid() const noexcept
        {
            return atlas != nullptr && skeletonData != nullptr;
        }
    };

    // 此资源对象对应一个 unit_id，并包含准备区和战斗区两套 skeleton。
    struct UnitAnimationAsset
    {
        std::string unitId;
        UnitAnimationSkeletonAsset preparation;
        UnitAnimationSkeletonAsset combat;

        bool valid() const noexcept
        {
            return preparation.valid() && combat.valid();
        }
    };

    using UnitAnimationAssetPtr = std::shared_ptr<UnitAnimationAsset>;

    // 此仓库负责按单位缓存 Spine 资源，失败时返回空指针而不是让游戏崩溃。
    class SpineAssetRepository final
    {
    public:
        explicit SpineAssetRepository(std::filesystem::path dataDirectory);

        UnitAnimationAssetPtr load(
            const std::string& unitId,
            std::string& errorMessage);

        void clear() noexcept;

    private:
        std::filesystem::path dataDirectory_;
        std::unordered_map<std::string, UnitAnimationAssetPtr> cache_;

        static bool loadSkeleton(
            const std::filesystem::path& directory,
            const std::string& stem,
            UnitAnimationSkeletonAsset& output,
            std::string& errorMessage);
    };
}