#include "game/rendering/MapRenderer.hpp"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/VertexArray.hpp>

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
                return sf::Color(55, 58, 66);
            }
            if (tile == 'A')
            {
                return sf::Color(55, 105, 175);
            }
            if (tile == 'B')
            {
                return sf::Color(170, 70, 70);
            }
            if (tile == 'X')
            {
                return sf::Color(40, 75, 145);
            }
            if (tile == 'Y')
            {
                return sf::Color(145, 45, 55);
            }
            return sf::Color(100, 110, 105);
        }
    }

    // 此函数先绘制全部格子，再叠加路线，最后绘制棋盘外框。
    void MapRenderer::draw(
        sf::RenderTarget& target,
        const core::MapDefinition& map,
        const BoardTransform& transform,
        const bool showRoutes)
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
                cell.setOutlineColor(sf::Color(35, 38, 46));
                target.draw(cell);
            }
        }

        // 此代码块在调试开关开启时按阵营颜色连接每条路线的格子中心。
        if (showRoutes)
        {
            for (const core::Route& route : map.routes)
            {
                sf::VertexArray line(sf::LineStrip, route.points.size());
                const sf::Color color = route.side == core::MapSide::A
                    ? sf::Color(85, 190, 255, 210)
                    : sf::Color(255, 120, 120, 210);
                for (std::size_t index = 0; index < route.points.size(); ++index)
                {
                    line[index].position = transform.cellCenter(route.points[index]);
                    line[index].color = color;
                }
                target.draw(line);
            }
        }

        // 此代码块为实际棋盘区域绘制清晰外边界。
        const sf::FloatRect boardBounds = transform.boardBounds();
        sf::RectangleShape outline(
            sf::Vector2f(boardBounds.width, boardBounds.height));
        outline.setPosition(boardBounds.left, boardBounds.top);
        outline.setFillColor(sf::Color::Transparent);
        outline.setOutlineThickness(2.0F);
        outline.setOutlineColor(sf::Color(155, 165, 185));
        target.draw(outline);
    }
}
