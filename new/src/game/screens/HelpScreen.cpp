#include "game/screens/HelpScreen.hpp"

#include "game/ui/UiTheme.hpp"

namespace autochess::game
{
    // 此构造函数建立帮助标题、操作说明和返回按钮。
    HelpScreen::HelpScreen(const sf::Font& font)
        : backButton_(font, sf::FloatRect(490.0F, 610.0F, 300.0F, 58.0F), L"返回主菜单")
    {
        title_.setFont(font);
        title_.setString(L"游戏帮助");
        ui::setTextSize(title_, 44);
        title_.setFillColor(ui::TextPrimary);
        ui::centerTextAtTop(title_, 640.0F, 70.0F);

        content_.setFont(font);
        content_.setString(
            L"1. 依次选择地图、自己的分队和电脑策略。\n\n"
            L"2. 点击右侧单位卡片购买单位，点击刷新更换六个选项。\n\n"
            L"3. 按住单位拖到蓝色部署格；拖回备用槽可以撤回。\n\n"
            L"4. 同类型同等级单位拖到一起可以合成升级。\n\n"
            L"5. 拖到出售区可以出售；死亡单位可点击复活。\n\n"
            L"6. 点击开始战斗，或等待准备倒计时自动归零。");
        ui::setTextSize(content_, 24);
        content_.setFillColor(ui::TextSecondary);
        content_.setPosition(250.0F, 165.0F);
        content_.setLineSpacing(1.15F);
    }

    // 此函数只在返回按钮完成一次点击时生成导航动作。
    void HelpScreen::handleEvent(const sf::Event& event)
    {
        // 此代码块将返回按钮点击记录为一次待消费动作。
        if (backButton_.handleEvent(event))
        {
            pendingAction_.kind = UiActionKind::BackToMenu;
        }
    }

    // 此函数绘制帮助页的标题、正文和返回按钮。
    void HelpScreen::draw(sf::RenderTarget& target) const
    {
        target.draw(title_);
        target.draw(content_);
        backButton_.draw(target);
    }

    // 此函数返回一次帮助页动作并清空内部消息。
    UiAction HelpScreen::takeAction()
    {
        UiAction action = pendingAction_;
        pendingAction_ = UiAction{};
        return action;
    }
}
