#pragma once

#include "core/config/ConfigBundleLoader.hpp"
#include "core/match/Match.hpp"

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>

#include <memory>

namespace autochess::game
{
    // 此类集中管理游戏窗口、启动资源和六十分之一秒固定更新循环。
    class GameApp
    {
    public:
        GameApp();

        // 此函数完成启动检查并运行窗口主循环，失败时返回非零退出码。
        int run();

    private:
        // 此函数从正式数据目录加载全部经过校验的核心配置。
        bool loadConfiguration();

        // 此函数按稳定候选顺序加载 Windows 中文字体。
        bool loadChineseFont();

        // 此函数处理一次窗口事件并保持关闭操作随时可用。
        void handleEvent(const sf::Event& event);

        // 此函数仅在核心处于可推进阶段时执行一个固定模拟帧。
        void fixedUpdate();

        // 此函数绘制第七天应用骨架的资源加载成功画面。
        void render();

        sf::RenderWindow window_;
        sf::Font font_;
        core::ConfigBundle config_;
        std::unique_ptr<core::Match> match_;
        sf::Clock frameClock_;
        float accumulatorSeconds_ = 0.0F;
    };
}
