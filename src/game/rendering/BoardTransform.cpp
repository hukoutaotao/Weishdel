#include "game/rendering/BoardTransform.hpp"

#include <algorithm>
#include <cmath>

namespace autochess::game
{
    // 此函数使用较小缩放轴确保整张地图始终位于给定视口内。
    void BoardTransform::configure(
        const sf::FloatRect& viewport,
        const int mapWidth,
        const int mapHeight) noexcept
    {
        mapWidth_ = mapWidth;
        mapHeight_ = mapHeight;
        // 此代码块在无效尺寸下清空变换，阻止除零和错误命中。
        if (mapWidth_ <= 0 || mapHeight_ <= 0
            || viewport.width <= 0.0F || viewport.height <= 0.0F)
        {
            tileSize_ = 0.0F;
            origin_ = sf::Vector2f();
            return;
        }

        tileSize_ = std::min(
            viewport.width / static_cast<float>(mapWidth_),
            viewport.height / static_cast<float>(mapHeight_));
        const float boardWidth = tileSize_ * static_cast<float>(mapWidth_);
        const float boardHeight = tileSize_ * static_cast<float>(mapHeight_);
        origin_ = sf::Vector2f(
            viewport.left + (viewport.width - boardWidth) / 2.0F,
            viewport.top + (viewport.height - boardHeight) / 2.0F);
    }

    // 此函数要求正格子尺寸和正地图尺寸同时成立。
    bool BoardTransform::valid() const noexcept
    {
        return tileSize_ > 0.0F && mapWidth_ > 0 && mapHeight_ > 0;
    }

    // 此函数将一个格子坐标转换为完整像素矩形。
    sf::FloatRect BoardTransform::cellBounds(
        const core::GridPosition position) const noexcept
    {
        return sf::FloatRect(
            origin_.x + static_cast<float>(position.x) * tileSize_,
            origin_.y + static_cast<float>(position.y) * tileSize_,
            tileSize_,
            tileSize_);
    }

    // 此函数在格子左上角基础上偏移半格得到中心点。
    sf::Vector2f BoardTransform::cellCenter(
        const core::GridPosition position) const noexcept
    {
        const sf::FloatRect bounds = cellBounds(position);
        return sf::Vector2f(
            bounds.left + bounds.width / 2.0F,
            bounds.top + bounds.height / 2.0F);
    }

    // 此函数先验证棋盘边界再向下取整获得格子索引。
    std::optional<core::GridPosition> BoardTransform::pixelToGrid(
        const sf::Vector2f pixel) const noexcept
    {
        // 此代码块拒绝未配置变换或实际棋盘矩形外的像素。
        if (!valid() || !boardBounds().contains(pixel))
        {
            return std::nullopt;
        }

        const int gridX = static_cast<int>(
            std::floor((pixel.x - origin_.x) / tileSize_));
        const int gridY = static_cast<int>(
            std::floor((pixel.y - origin_.y) / tileSize_));
        // 此代码块处理右下边界浮点误差导致的越界索引。
        if (gridX < 0 || gridY < 0
            || gridX >= mapWidth_ || gridY >= mapHeight_)
        {
            return std::nullopt;
        }
        return core::GridPosition{gridX, gridY};
    }

    // 此函数根据当前原点、格子尺寸和地图尺寸构造实际棋盘区域。
    sf::FloatRect BoardTransform::boardBounds() const noexcept
    {
        return sf::FloatRect(
            origin_.x,
            origin_.y,
            tileSize_ * static_cast<float>(mapWidth_),
            tileSize_ * static_cast<float>(mapHeight_));
    }
}
