#include "game/screens/MainMenuScreen.hpp"

namespace autochess::game
{
    // 此构造函数建立固定布局的标题和三个主菜单按钮。
    MainMenuScreen::MainMenuScreen(const sf::Font& font)
        : startButton_(font, sf::FloatRect(490.0F, 330.0F, 300.0F, 64.0F), L"开始游戏"),
          helpButton_(font, sf::FloatRect(490.0F, 420.0F, 300.0F, 64.0F), L"帮助"),
          exitButton_(font, sf::FloatRect(490.0F, 510.0F, 300.0F, 64.0F), L"退出")
    {
        title_.setFont(font);
        title_.setString(L"自走棋对战系统");
        title_.setCharacterSize(52);
        title_.setFillColor(sf::Color(235, 238, 248));
        title_.setPosition(420.0F, 135.0F);

        subtitle_.setFont(font);
        subtitle_.setString(L"C++17 + SFML 课程项目");
        subtitle_.setCharacterSize(24);
        subtitle_.setFillColor(sf::Color(155, 175, 210));
        subtitle_.setPosition(500.0F, 225.0F);
    }

    // 此函数把每个完整点击转换为一个待消费的导航动作。
    void MainMenuScreen::handleEvent(const sf::Event& event)
    {
        // 此代码块按按钮优先级记录本次事件产生的唯一动作。
        if (startButton_.handleEvent(event))
        {
            pendingAction_.kind = UiActionKind::StartGame;
        }
        else if (helpButton_.handleEvent(event))
        {
            pendingAction_.kind = UiActionKind::ShowHelp;
        }
        else if (exitButton_.handleEvent(event))
        {
            pendingAction_.kind = UiActionKind::ExitApplication;
        }
    }

    // 此函数按标题、说明和按钮顺序绘制主菜单。
    void MainMenuScreen::draw(sf::RenderTarget& target) const
    {
        target.draw(title_);
        target.draw(subtitle_);
        startButton_.draw(target);
        helpButton_.draw(target);
        exitButton_.draw(target);
    }

    // 此函数返回一次主菜单动作并保证同一点击不会重复处理。
    UiAction MainMenuScreen::takeAction()
    {
        UiAction action = pendingAction_;
        pendingAction_ = UiAction{};
        return action;
    }
}
