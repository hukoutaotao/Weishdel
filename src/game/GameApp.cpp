#include "game/GameApp.hpp"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Window/Event.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>

namespace autochess::game
{
    namespace
    {
        constexpr unsigned int WindowWidth = 1280;
        constexpr unsigned int WindowHeight = 720;
        constexpr float FixedStepSeconds = 1.0F / 60.0F;
        constexpr float MaximumFrameSeconds = 0.25F;
    }

    // 此构造函数创建固定尺寸窗口并限制空闲渲染帧率。
    GameApp::GameApp()
        : window_(
              sf::VideoMode(WindowWidth, WindowHeight),
              "AutoChess")
    {
        window_.setFramerateLimit(60);
    }

    // 此函数完成资源加载后持续处理事件、固定更新和绘制。
    int GameApp::run()
    {
        // 此代码块在任一启动资源失败时给出错误并停止进入主循环。
        if (!loadConfiguration() || !loadChineseFont())
        {
            return 1;
        }

        // 此代码块在窗口存活期间限制累计时间并按固定步长更新核心。
        while (window_.isOpen())
        {
            sf::Event event{};
            while (window_.pollEvent(event))
            {
                handleEvent(event);
            }

            const float frameSeconds = std::min(
                frameClock_.restart().asSeconds(),
                MaximumFrameSeconds);
            accumulatorSeconds_ += frameSeconds;
            while (accumulatorSeconds_ >= FixedStepSeconds)
            {
                fixedUpdate();
                accumulatorSeconds_ -= FixedStepSeconds;
            }

            render();
        }

        return 0;
    }

    // 此函数加载 CMake 提供的正式数据目录并输出带路径和行号的错误。
    bool GameApp::loadConfiguration()
    {
        core::ConfigError error;
        const bool loaded = core::ConfigBundleLoader::load(
            std::filesystem::path(AUTOCHESS_DATA_DIR),
            config_,
            error);
        // 此代码块在配置失败时输出能够直接定位文件的诊断信息。
        if (!loaded)
        {
            std::cerr << "[CONFIG] " << error.sourcePath.string();
            if (error.line > 0)
            {
                std::cerr << ':' << error.line;
            }
            std::cerr << ' ' << error.message << '\n';
            return false;
        }

        return true;
    }

    // 此函数尝试系统常见中文字体并报告全部失败的候选路径。
    bool GameApp::loadChineseFont()
    {
        const std::array<std::filesystem::path, 3> candidates = {
            std::filesystem::path("C:/Windows/Fonts/msyh.ttc"),
            std::filesystem::path("C:/Windows/Fonts/simhei.ttf"),
            std::filesystem::path("C:/Windows/Fonts/Deng.ttf")};

        // 此代码块按优先级使用第一个能够被 SFML 成功读取的字体。
        for (const std::filesystem::path& path : candidates)
        {
            if (font_.loadFromFile(path.string()))
            {
                return true;
            }
        }

        // 此代码块列出所有字体候选并以明确错误结束启动过程。
        std::cerr << "[FONT] 无法加载中文字体，已尝试：\n";
        for (const std::filesystem::path& path : candidates)
        {
            std::cerr << "  " << path.string() << '\n';
        }
        return false;
    }

    // 此函数响应系统关闭事件且暂不引入任何游戏界面操作。
    void GameApp::handleEvent(const sf::Event& event)
    {
        // 此代码块确保窗口关闭按钮始终立即生效。
        if (event.type == sf::Event::Closed)
        {
            window_.close();
        }
    }

    // 此函数把固定步长只发送给准备、战斗和结算阶段的核心对局。
    void GameApp::fixedUpdate()
    {
        // 此代码块在尚未创建对局时保持资源检查页面静止。
        if (match_ == nullptr)
        {
            return;
        }

        const core::MatchPhase phase = match_->phase();
        // 此代码块排除菜单选择和最终结果阶段，避免准备倒计时提前消耗。
        if (phase == core::MatchPhase::Preparation
            || phase == core::MatchPhase::Combat
            || phase == core::MatchPhase::RoundSettlement)
        {
            match_->step();
        }
    }

    // 此函数显示配置与中文字体均已成功加载的可视化检查结果。
    void GameApp::render()
    {
        window_.clear(sf::Color(30, 30, 40));

        // 此代码块使用中文字体绘制可直接观察的第七天启动基线。
        sf::Text title;
        title.setFont(font_);
        title.setString(L"自走棋对战系统\n第 7 天资源加载成功");
        title.setCharacterSize(36);
        title.setFillColor(sf::Color(235, 235, 245));
        title.setPosition(390.0F, 285.0F);
        window_.draw(title);

        window_.display();
    }
}
