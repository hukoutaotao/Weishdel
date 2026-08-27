#include "game/animation/SpineAssetRepository.hpp"

#include <spine/SkeletonBinary.h>

#include <fstream>
#include <sstream>
#include <utility>

namespace autochess::game
{
    namespace
    {
        bool hasRenderableTexture(spine::Atlas& atlas)
        {
            for (std::size_t index = 0; index < atlas.getPages().size(); ++index)
            {
                if (atlas.getPages()[index] == nullptr
                    || atlas.getPages()[index]->getRendererObject() == nullptr)
                {
                    return false;
                }
            }
            return atlas.getPages().size() > 0;
        }

        std::string spineError(
            const std::filesystem::path& skeletonPath,
            const spine::String& error)
        {
            std::ostringstream stream;
            stream << skeletonPath.string() << ": "
                   << (error.buffer() == nullptr ? "无法解析 skeleton" : error.buffer());
            return stream.str();
        }
    }

    SpineAssetRepository::SpineAssetRepository(
        std::filesystem::path dataDirectory)
        : dataDirectory_(std::move(dataDirectory))
    {
    }

    UnitAnimationAssetPtr SpineAssetRepository::load(
        const std::string& unitId,
        std::string& errorMessage)
    {
        const auto cached = cache_.find(unitId);
        if (cached != cache_.end())
        {
            return cached->second;
        }

        if (unitId.empty())
        {
            errorMessage = "Spine 单位 ID 为空";
            return nullptr;
        }

        const std::filesystem::path directory =
            dataDirectory_ / "animations" / "units" / unitId;
        auto asset = std::make_shared<UnitAnimationAsset>();
        asset->unitId = unitId;
        if (!loadSkeleton(directory, unitId + "1", asset->preparation, errorMessage)
            || !loadSkeleton(directory, unitId + "2", asset->combat, errorMessage))
        {
            return nullptr;
        }

        cache_.emplace(unitId, asset);
        return asset;
    }

    void SpineAssetRepository::clear() noexcept
    {
        cache_.clear();
    }

    bool SpineAssetRepository::loadSkeleton(
        const std::filesystem::path& directory,
        const std::string& stem,
        UnitAnimationSkeletonAsset& output,
        std::string& errorMessage)
    {
        const std::filesystem::path atlasPath = directory / (stem + ".atlas");
        const std::filesystem::path skeletonPath = directory / (stem + ".skel");
        if (!std::filesystem::is_regular_file(atlasPath)
            || !std::filesystem::is_regular_file(skeletonPath))
        {
            errorMessage = "缺少 Spine 文件：" + atlasPath.string()
                + " 或 " + skeletonPath.string();
            return false;
        }

        output.textureLoader = std::make_unique<spine::SFMLTextureLoader>();
        output.atlas = std::make_unique<spine::Atlas>(
            spine::String(atlasPath.string().c_str()),
            output.textureLoader.get());
        if (!hasRenderableTexture(*output.atlas))
        {
            errorMessage = "无法加载 Spine 纹理：" + atlasPath.string();
            output.atlas.reset();
            output.textureLoader.reset();
            return false;
        }

        spine::SkeletonBinary binary(output.atlas.get());
        output.skeletonData.reset(binary.readSkeletonDataFile(
            spine::String(skeletonPath.string().c_str())));
        if (output.skeletonData == nullptr)
        {
            errorMessage = spineError(skeletonPath, binary.getError());
            output.atlas.reset();
            output.textureLoader.reset();
            return false;
        }
        output.manifest = UnitAnimationManifest::fromSkeletonData(
            *output.skeletonData);
        return true;
    }
}