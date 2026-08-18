#pragma once

#include "core/config/ConfigBundleLoader.hpp"
#include "core/match/Match.hpp"
#include "core/ai/AiController.hpp"
#include "game/controllers/HumanController.hpp"
#include "game/screens/Screen.hpp"

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>

#include <cstdint>
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

        // 此函数消费当前页面动作并执行应用级导航。
        void processUiAction();

        // 此函数切换到全新主菜单页面。
        void showMainMenu();

        // 此函数切换到全新帮助页面。
        void showHelp();

        // 此函数创建全新核心对局和双方第七天控制器。
        void startNewGame();

        // 此函数让双方控制器基于最新快照各提交至多一条命令。
        void processControllerCommands();

        // 此函数把最新 A 方只读快照交给对局页面。
        void updateMatchScreen();

        // 此函数仅在核心处于可推进阶段时执行一个固定模拟帧。
        void fixedUpdate();

        // 此函数绘制第七天应用骨架的资源加载成功画面。
        void render();

        // 此函数在当前选择不变的情况下事务式重建第一回合对局。
        void restartCurrentGame();

        // 此函数清除离开对局页面后不应继续更新的核心对象。
        void clearMatchState() noexcept;

        sf::RenderWindow window_;
        sf::Font font_;
        core::ConfigBundle config_;
        std::unique_ptr<core::Match> match_;
        std::unique_ptr<HumanController> humanController_;
        // 此成员保存注入正式配置的统一电脑控制器。
        std::unique_ptr<core::AiController> computerController_;
        std::unique_ptr<Screen> screen_;
        sf::Clock frameClock_;
        float accumulatorSeconds_ = 0.0F;
        // 此代码块保存暂停和结算停留的应用层固定帧状态。
        bool paused_ = false;
        std::uint64_t settlementHoldFrames_ = 0;
    };
}
