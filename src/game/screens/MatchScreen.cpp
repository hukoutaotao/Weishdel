#include "game/screens/MatchScreen.hpp"

namespace autochess::game
{
    namespace
    {
        // 此函数把核心 UTF-8 文本安全转换为 SFML Unicode 字符串。
        sf::String fromUtf8(const std::string& text)
        {
            return sf::String::fromUtf8(text.begin(), text.end());
        }

        // 此函数返回三种 AI 策略对应的固定中文标题。
        sf::String strategyName(const core::AiStrategyKind strategy)
        {
            // 此代码块把未知值以外的三种正式策略转换为用户可读名称。
            if (strategy == core::AiStrategyKind::Offensive)
            {
                return L"进攻型";
            }
            if (strategy == core::AiStrategyKind::Defensive)
            {
                return L"防守型";
            }
            return L"路线型";
        }
    }

    // 此构造函数建立选择流程共用的标题、提示和消息文本。
    MatchScreen::MatchScreen(const sf::Font& font)
        : font_(font)
    {
        title_.setFont(font_);
        title_.setCharacterSize(42);
        title_.setFillColor(sf::Color(235, 238, 248));
        title_.setPosition(470.0F, 80.0F);

        hint_.setFont(font_);
        hint_.setCharacterSize(22);
        hint_.setFillColor(sf::Color(160, 180, 215));
        hint_.setPosition(425.0F, 150.0F);

        message_.setFont(font_);
        message_.setCharacterSize(22);
        message_.setPosition(390.0F, 625.0F);
    }

    // 此函数把一次选择按钮点击转换为待提交核心命令。
    void MatchScreen::handleEvent(const sf::Event& event)
    {
        // 此代码块按配置顺序检测全部当前阶段选择按钮。
        for (const Choice& choice : choices_)
        {
            if (choice.button != nullptr && choice.button->handleEvent(event))
            {
                submitChoice(choice);
                break;
            }
        }
    }

    // 此函数绘制选择标题、阶段说明、全部选项和最近命令结果。
    void MatchScreen::draw(sf::RenderTarget& target) const
    {
        target.draw(title_);
        target.draw(hint_);
        // 此代码块按配置顺序绘制当前阶段可用的全部按钮。
        for (const Choice& choice : choices_)
        {
            if (choice.button != nullptr)
            {
                choice.button->draw(target);
            }
        }
        target.draw(message_);
    }

    // 此函数返回一次选择命令动作并清除内部待处理状态。
    UiAction MatchScreen::takeAction()
    {
        UiAction action = pendingAction_;
        pendingAction_ = UiAction{};
        return action;
    }

    // 此函数保存最新快照并仅在阶段变化时重建稳定按钮布局。
    void MatchScreen::updateView(const core::ReadOnlyGameView& view)
    {
        view_ = view;
        // 此代码块避免同一阶段每个渲染帧重复分配按钮。
        if (builtPhase_ != view_.phase)
        {
            builtPhase_ = view_.phase;
            rebuildChoices();
        }
    }

    // 此函数使用颜色区分成功和失败的核心中文结果。
    void MatchScreen::showCommandResult(const core::CommandResult& result)
    {
        message_.setString(fromUtf8(result.message));
        message_.setFillColor(
            result.success
                ? sf::Color(100, 215, 135)
                : sf::Color(240, 105, 105));
    }

    // 此函数根据核心选择列表建立动态数量的选择按钮。
    void MatchScreen::rebuildChoices()
    {
        choices_.clear();
        message_.setString(L"");

        const float buttonLeft = 390.0F;
        const float buttonWidth = 500.0F;
        const float buttonHeight = 64.0F;
        const float firstTop = 225.0F;
        const float gap = 82.0F;

        // 此代码块分别为地图、分队和 AI 阶段设置明确标题与选项来源。
        if (view_.phase == core::MatchPhase::MapSelection)
        {
            title_.setString(L"选择地图");
            hint_.setString(L"请选择本局使用的战场");
            for (std::size_t index = 0; index < view_.maps.size(); ++index)
            {
                const core::SelectionOptionView& option = view_.maps[index];
                Choice choice;
                choice.id = option.id;
                choice.button = std::make_unique<Button>(
                    font_,
                    sf::FloatRect(
                        buttonLeft,
                        firstTop + gap * static_cast<float>(index),
                        buttonWidth,
                        buttonHeight),
                    fromUtf8(option.name));
                choices_.push_back(std::move(choice));
            }
        }
        else if (view_.phase == core::MatchPhase::FactionSelection)
        {
            title_.setString(L"选择分队");
            hint_.setString(L"分队会影响部署上限、价格和单位属性");
            for (std::size_t index = 0; index < view_.factions.size(); ++index)
            {
                const core::SelectionOptionView& option = view_.factions[index];
                Choice choice;
                choice.id = option.id;
                choice.button = std::make_unique<Button>(
                    font_,
                    sf::FloatRect(
                        buttonLeft,
                        firstTop + gap * static_cast<float>(index),
                        buttonWidth,
                        buttonHeight),
                    fromUtf8(option.name));
                choices_.push_back(std::move(choice));
            }
        }
        else if (view_.phase == core::MatchPhase::AiSelection)
        {
            title_.setString(L"选择电脑策略");
            hint_.setString(L"第 9 天将接入完整策略；今天先验证合法对局流程");
            for (std::size_t index = 0; index < view_.aiStrategies.size(); ++index)
            {
                Choice choice;
                choice.strategy = view_.aiStrategies[index];
                choice.button = std::make_unique<Button>(
                    font_,
                    sf::FloatRect(
                        buttonLeft,
                        firstTop + gap * static_cast<float>(index),
                        buttonWidth,
                        buttonHeight),
                    strategyName(choice.strategy));
                choices_.push_back(std::move(choice));
            }
        }
        else
        {
            title_.setString(L"准备阶段");
            hint_.setString(L"选择流程完成，准备界面将在步骤 7.5 开始绘制");
        }
    }

    // 此函数只根据当前阶段构造一条 A 方地图、分队或策略命令。
    void MatchScreen::submitChoice(const Choice& choice)
    {
        core::GameCommand command;
        command.actor = core::MapSide::A;
        // 此代码块把三个选择阶段分别映射到现有统一命令负载。
        if (view_.phase == core::MatchPhase::MapSelection)
        {
            command.payload = core::SelectMapCommand{choice.id};
        }
        else if (view_.phase == core::MatchPhase::FactionSelection)
        {
            command.payload = core::SelectFactionCommand{choice.id};
        }
        else if (view_.phase == core::MatchPhase::AiSelection)
        {
            command.payload = core::SelectAiStrategyCommand{choice.strategy};
        }
        else
        {
            return;
        }

        pendingAction_.kind = UiActionKind::SubmitCommand;
        pendingAction_.command = std::move(command);
    }
}
