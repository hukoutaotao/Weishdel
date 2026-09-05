#include "game/rendering/MapRenderer.hpp"

#include "game/ui/UiTheme.hpp"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/VertexArray.hpp>

#include <algorithm>
#include <cmath>

namespace autochess::game
{
    namespace
    {
        // 此函数为地图文件中的每种字符返回固定且易区分的颜色。
        sf::Color tileColor(const char tile)
        {
            // 此代码块区分障碍、双方部署格、双方守卫和普通地面。
            if (tile == '#')
            {
                return sf::Color(38, 38, 40);
            }
            if (tile == 'A')
            {
                return ui::Friendly;
            }
            if (tile == 'B')
            {
                return ui::Enemy;
            }
            if (tile == 'X')
            {
                return sf::Color(54, 77, 105);
            }
            if (tile == 'Y')
            {
                return sf::Color(108, 53, 59);
            }
            return sf::Color(70, 73, 71);
        }

        // 此函数使用短线和间隔绘制一条不依赖纹理的白色虚线路线。
        void drawDashedRoute(
            sf::RenderTarget& target,
            const core::Route& route,
            const BoardTransform& transform)
        {
            if (route.points.size() < 2)
            {
                return;
            }

            const float tileSize = transform.cellBounds({0, 0}).width;
            const float dashLength = std::max(4.0F, tileSize * 0.24F);
            const float gapLength = std::max(3.0F, tileSize * 0.14F);
            sf::VertexArray dashes(sf::Lines);

            // 此代码块逐段插入白色短线，路线拐角处仍严格经过格子中心。
            for (std::size_t index = 1; index < route.points.size(); ++index)
            {
                const sf::Vector2f start =
                    transform.cellCenter(route.points[index - 1]);
                const sf::Vector2f end =
                    transform.cellCenter(route.points[index]);
                const sf::Vector2f delta(end.x - start.x, end.y - start.y);
                const float length = std::sqrt(
                    delta.x * delta.x + delta.y * delta.y);
                if (length <= 0.0F)
                {
                    continue;
                }

                for (float offset = 0.0F;
                     offset < length;
                     offset += dashLength + gapLength)
                {
                    const float dashEnd = std::min(offset + dashLength, length);
                    const float startRatio = offset / length;
                    const float endRatio = dashEnd / length;
                    dashes.append(sf::Vertex(
                        sf::Vector2f(
                            start.x + delta.x * startRatio,
                            start.y + delta.y * startRatio),
                        ui::withAlpha(ui::TextPrimary, 225)));
                    dashes.append(sf::Vertex(
                        sf::Vector2f(
                            start.x + delta.x * endRatio,
                            start.y + delta.y * endRatio),
                        ui::withAlpha(ui::TextPrimary, 225)));
                }
            }
            target.draw(dashes);
        }
    }

    // 此函数先绘制全部格子，再叠加路线或部署预览，最后绘制棋盘外框。
    void MapRenderer::draw(
        sf::RenderTarget& target,
        const core::MapDefinition& map,
        const BoardTransform& transform,
        const std::optional<core::GridPosition>& deploymentPreview)
    {
        // 此代码块拒绝无效变换或不完整地图数据。
        if (!transform.valid()
            || map.gridRows.size() != static_cast<std::size_t>(map.height))
        {
            return;
        }

        // 此代码块逐格绘制地形色块和细网格线。
        for (int y = 0; y < map.height; ++y)
        {
            for (int x = 0; x < map.width; ++x)
            {
                const sf::FloatRect bounds = transform.cellBounds({x, y});
                sf::RectangleShape cell(
                    sf::Vector2f(bounds.width - 1.0F, bounds.height - 1.0F));
                cell.setPosition(bounds.left + 0.5F, bounds.top + 0.5F);
                cell.setFillColor(tileColor(
                    map.gridRows[static_cast<std::size_t>(y)]
                                [static_cast<std::size_t>(x)]));
                cell.setOutlineThickness(1.0F);
                cell.setOutlineColor(ui::Background);
                target.draw(cell);
            }
        }

        // 此代码块把当前悬停部署格压暗，并只绘制该格对应的白色虚线路线。
        if (deploymentPreview.has_value())
        {
            const sf::FloatRect bounds =
                transform.cellBounds(deploymentPreview.value());
            sf::RectangleShape shade(
                sf::Vector2f(bounds.width - 1.0F, bounds.height - 1.0F));
            shade.setPosition(bounds.left + 0.5F, bounds.top + 0.5F);
            shade.setFillColor(sf::Color(0, 0, 0, 120));
            target.draw(shade);

            const auto route = std::find_if(
                map.routes.begin(),
                map.routes.end(),
                [&deploymentPreview](const core::Route& candidate) {
                    return candidate.side == core::MapSide::A
                        && candidate.start == deploymentPreview.value();
                });
            if (route != map.routes.end())
            {
                drawDashedRoute(target, *route, transform);
            }
        }

        // 此代码块为实际棋盘区域绘制清晰外边界。
        const sf::FloatRect boardBounds = transform.boardBounds();
        sf::RectangleShape outline(
            sf::Vector2f(boardBounds.width, boardBounds.height));
        outline.setPosition(boardBounds.left, boardBounds.top);
        outline.setFillColor(sf::Color::Transparent);
        outline.setOutlineThickness(2.0F);
        outline.setOutlineColor(ui::BorderStrong);
        target.draw(outline);
    }
}
