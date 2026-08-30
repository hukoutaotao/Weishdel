#include "game/GameApp.hpp"

#include "game/screens/HelpScreen.hpp"
#include "game/screens/MainMenuScreen.hpp"
#include "game/screens/MatchScreen.hpp"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/WindowStyle.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <cmath>
#include <utility>

namespace autochess::game
{
    namespace
    {
        constexpr unsigned int LogicalWidth = 1280;
        constexpr unsigned int LogicalHeight = 720;
        constexpr unsigned int WindowWidth = LogicalWidth * 2;
        constexpr unsigned int WindowHeight = LogicalHeight * 2;
        constexpr float FixedStepSeconds = 1.0F / 60.0F;
        constexpr float MaximumFrameSeconds = 0.25F;
    }

    // 此构造函数创建放大一倍的窗口，但保留页面使用的逻辑坐标系。
    GameApp::GameApp(std::filesystem::path dataDirectory)
        : dataDirectory_(std::move(dataDirectory)),
          window_(
              sf::VideoMode(WindowWidth, WindowHeight),
              "AutoChess"),
          logicalView_(sf::FloatRect(
              0.0F,
              0.0F,
              static_cast<float>(LogicalWidth),
              static_cast<float>(LogicalHeight)))
    {
        window_.setView(logicalView_);
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

        // 此代码块在资源可用后创建第一个可交互主菜单页面。
        showMainMenu();

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

    // 此函数加载入口选定的数据目录并输出带路径和行号的错误。
    bool GameApp::loadConfiguration()
    {
        core::ConfigError error;
        const bool loaded = core::ConfigBundleLoader::load(
            dataDirectory_,
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
            return;
        }

        // 此代码块保持逻辑视图覆盖整个放大后的窗口，避免调整窗口尺寸后
        // 页面仍按旧默认视图绘制。
        if (event.type == sf::Event::Resized)
        {
            window_.setView(logicalView_);
        }

        // 此代码块把物理像素鼠标坐标转换回 1280×720 逻辑坐标，
        // 再交给页面处理，保证放大窗口不影响按钮、拖拽和选中操作。
        if (screen_ != nullptr)
        {
            sf::Event screenEvent = event;
            const auto toLogical = [this](const sf::Vector2i pixel) {
                return window_.mapPixelToCoords(pixel, logicalView_);
            };
            if (event.type == sf::Event::MouseButtonPressed
                || event.type == sf::Event::MouseButtonReleased)
            {
                const sf::Vector2f logical = toLogical(sf::Vector2i(
                    event.mouseButton.x,
                    event.mouseButton.y));
                screenEvent.mouseButton.x = static_cast<int>(
                    std::lround(logical.x));
                screenEvent.mouseButton.y = static_cast<int>(
                    std::lround(logical.y));
            }
            else if (event.type == sf::Event::MouseMoved)
            {
                const sf::Vector2f logical = toLogical(sf::Vector2i(
                    event.mouseMove.x,
                    event.mouseMove.y));
                screenEvent.mouseMove.x = static_cast<int>(
                    std::lround(logical.x));
                screenEvent.mouseMove.y = static_cast<int>(
                    std::lround(logical.y));
            }
            screen_->handleEvent(screenEvent);
            processUiAction();
        }
    }

    // 此函数将屏幕动作转换为页面切换、退出或下一步占位行为。
    void GameApp::processUiAction()
    {
        // 此代码块在没有当前页面时拒绝读取动作。
        if (screen_ == nullptr)
        {
            return;
        }

        const UiAction action = screen_->takeAction();
        // 此代码块保证空动作不会触发任何应用状态变化。
        if (action.kind == UiActionKind::None)
        {
            return;
        }

        // 此代码块执行第七天菜单阶段已经具备的应用导航。
        if (action.kind == UiActionKind::ShowHelp)
        {
            showHelp();
        }
        else if (action.kind == UiActionKind::BackToMenu)
        {
            showMainMenu();
        }
        else if (action.kind == UiActionKind::ExitApplication)
        {
            window_.close();
        }
        else if (action.kind == UiActionKind::StartGame)
        {
            startNewGame();
        }
        else if (action.kind == UiActionKind::TogglePause
                 && match_ != nullptr
                 && (match_->phase() == core::MatchPhase::Preparation
                     || match_->phase() == core::MatchPhase::Combat))
        {
            // 此代码块把暂停动作限定在准备和战斗阶段并同步页面状态。
            paused_ = !paused_;
            if (auto* matchScreen = dynamic_cast<MatchScreen*>(screen_.get()))
            {
                matchScreen->setPaused(paused_);
            }
        }
        else if (action.kind == UiActionKind::RestartMatch)
        {
            restartCurrentGame();
        }
        else if (action.kind == UiActionKind::PlayAgain)
        {
            // 此代码块让结果页的再来一局严格回到地图选择阶段。
            startNewGame();
        }
        else if (action.kind == UiActionKind::SubmitCommand
                 && action.command.has_value()
                 && humanController_ != nullptr)
        {
            humanController_->enqueue(action.command.value());
        }
    }

    // 此函数用统一字体构造没有残留交互状态的主菜单。
    void GameApp::showMainMenu()
    {
        clearMatchState();
        screen_ = std::make_unique<MainMenuScreen>(font_);
    }

    // 此函数用统一字体构造没有残留交互状态的帮助页面。
    void GameApp::showHelp()
    {
        screen_ = std::make_unique<HelpScreen>(font_);
    }

    // 此函数重建对局及双方控制器并切换到地图选择页面。
    void GameApp::startNewGame()
    {
        match_ = std::make_unique<core::Match>(config_);
        humanController_ = std::make_unique<HumanController>(core::MapSide::A);
        // 此代码块让正式 AI 控制器负责 B 方并读取同一份配置快照。
        computerController_ = std::make_unique<core::AiController>(
            config_,
            core::MapSide::B);
        screen_ = std::make_unique<MatchScreen>(font_, config_, dataDirectory_);
        accumulatorSeconds_ = 0.0F;
        paused_ = false;
        settlementHoldFrames_ = 0;
        updateMatchScreen();
    }

    // 此函数在原选择不变时事务式创建新的第一回合 Match。
    void GameApp::restartCurrentGame()
    {
        if (match_ == nullptr)
        {
            return;
        }

        const core::ReadOnlyGameView previous =
            match_->viewFor(core::MapSide::A);
        if (previous.selectedMapId.empty()
            || previous.factionIdA.empty()
            || previous.factionIdB.empty()
            || previous.aiStrategy == core::AiStrategyKind::Unknown)
        {
            return;
        }

        auto restartedMatch = std::make_unique<core::Match>(config_);
        const auto mapResult = restartedMatch->submit({
            core::MapSide::A,
            core::SelectMapCommand{previous.selectedMapId}});
        const auto factionAResult = restartedMatch->submit({
            core::MapSide::A,
            core::SelectFactionCommand{previous.factionIdA}});
        const auto aiResult = restartedMatch->submit({
            core::MapSide::A,
            core::SelectAiStrategyCommand{previous.aiStrategy}});
        const auto factionBResult = restartedMatch->submit({
            core::MapSide::B,
            core::SelectFactionCommand{previous.factionIdB}});

        // 此代码块只有在四条选择命令全部成功后才替换旧对局。
        if (!mapResult.success || !factionAResult.success
            || !aiResult.success || !factionBResult.success)
        {
            if (auto* matchScreen = dynamic_cast<MatchScreen*>(screen_.get()))
            {
                matchScreen->showCommandResult(
                    !mapResult.success
                        ? mapResult
                        : (!factionAResult.success
                               ? factionAResult
                               : (!aiResult.success ? aiResult : factionBResult)));
            }
            return;
        }

        match_ = std::move(restartedMatch);
        humanController_ = std::make_unique<HumanController>(core::MapSide::A);
        // 此代码块在重开后重新注入配置，避免复用旧控制器状态。
        computerController_ = std::make_unique<core::AiController>(
            config_,
            core::MapSide::B);
        paused_ = false;
        settlementHoldFrames_ = 0;
        accumulatorSeconds_ = 0.0F;
        if (auto* matchScreen = dynamic_cast<MatchScreen*>(screen_.get()))
        {
            matchScreen->setPaused(false);
        }
        updateMatchScreen();
    }

    // 此函数清除对局对象并使主菜单不会继续推进隐藏核心状态。
    void GameApp::clearMatchState() noexcept
    {
        match_.reset();
        humanController_.reset();
        computerController_.reset();
        paused_ = false;
        settlementHoldFrames_ = 0;
        accumulatorSeconds_ = 0.0F;
    }

    // 此函数按 A 后 B 的稳定顺序提交控制器本帧产生的命令。
    void GameApp::processControllerCommands()
    {
        // 此代码块在任一对局组件尚未创建时保持静默。
        if (match_ == nullptr
            || humanController_ == nullptr
            || computerController_ == nullptr)
        {
            return;
        }

        const core::ReadOnlyGameView viewA =
            match_->viewFor(humanController_->side());
        // 此代码块提交真人本帧至多一条命令并把结果交给对局页面。
        if (const std::optional<core::GameCommand> command =
                humanController_->nextCommand(viewA))
        {
            const core::CommandResult result = match_->submit(command.value());
            if (auto* matchScreen = dynamic_cast<MatchScreen*>(screen_.get()))
            {
                matchScreen->showCommandResult(result);
            }
        }

        const core::ReadOnlyGameView viewB =
            match_->viewFor(computerController_->side());
        // 此代码块提交占位电脑本帧至多一条命令并记录意外失败。
        if (const std::optional<core::GameCommand> command =
                computerController_->nextCommand(viewB))
        {
            const core::CommandResult result = match_->submit(command.value());
            if (!result.success)
            {
                std::cerr << "[COMPUTER] " << result.message << '\n';
            }
        }
    }

    // 此函数向对局页面提供不暴露内部对象的最新 A 方快照。
    void GameApp::updateMatchScreen()
    {
        // 此代码块只在当前确实为对局页面且核心存在时复制快照。
        auto* matchScreen = dynamic_cast<MatchScreen*>(screen_.get());
        if (match_ != nullptr && matchScreen != nullptr)
        {
            matchScreen->updateView(match_->viewFor(core::MapSide::A));
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

        // 此代码块在暂停时保留渲染和事件循环但冻结所有核心时间。
        if (paused_)
        {
            return;
        }

        const core::MatchPhase phaseBefore = match_->phase();
        // 此代码块让回合结算页先保持两秒，再执行核心持久状态回写。
        if (phaseBefore == core::MatchPhase::RoundSettlement
            && settlementHoldFrames_ > 0)
        {
            --settlementHoldFrames_;
            // 这里只冻结核心结算时间，Spine 仍需逐帧更新，避免死亡时全场定格。
            if (auto* matchScreen = dynamic_cast<MatchScreen*>(screen_.get()))
            {
                matchScreen->updateAnimations(FixedStepSeconds);
                // 动画推进后重新同步，及时识别各自已经播完的死亡动作。
                updateMatchScreen();
            }
            return;
        }

        // 此代码块先执行本帧双方命令，再推进准备或战斗模拟。
        processControllerCommands();

        const core::MatchPhase phase = match_->phase();
        // 此代码块排除菜单选择和最终结果阶段，避免准备倒计时提前消耗。
        if (phase == core::MatchPhase::Preparation
            || phase == core::MatchPhase::Combat
            || phase == core::MatchPhase::RoundSettlement)
        {
            match_->step();
        }

        if (auto* matchScreen = dynamic_cast<MatchScreen*>(screen_.get()))
        {
            // 核心推进后先发布本固定帧的最新位置与行动序号，再推进动画。
            // 这样动作判断不会落后一帧，也不会在多次追帧时持续使用旧快照。
            updateMatchScreen();
            matchScreen->updateAnimations(FixedStepSeconds);
        }

        // 此代码块只在战斗刚结束时建立一次固定长度的结算停留。
        if (phase == core::MatchPhase::Combat
            && match_->phase() == core::MatchPhase::RoundSettlement)
        {
            settlementHoldFrames_ = 120;
        }
    }

    // 此函数显示配置与中文字体均已成功加载的可视化检查结果。
    void GameApp::render()
    {
        window_.clear(sf::Color(30, 30, 40));

        // 此代码块只绘制当前页面，避免页面之间残留控件。
        if (screen_ != nullptr)
        {
            screen_->draw(window_);
        }

        window_.display();
    }
}
