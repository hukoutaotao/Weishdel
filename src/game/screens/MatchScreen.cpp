#include "game/screens/MatchScreen.hpp"

#include "game/rendering/MapRenderer.hpp"
#include "game/rendering/UnitRenderer.hpp"

#include <sstream>
#include <utility>

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
    MatchScreen::MatchScreen(
        const sf::Font& font,
        const core::ConfigBundle& config)
        : font_(font),
          config_(config)
    {
        title_.setFont(font_);
        title_.setCharacterSize(42);
        title_.setFillColor(sf::Color(235, 238, 248));
        title_.setPosition(470.0F, 80.0F);

        hint_.setFont(font_);
        hint_.setCharacterSize(22);
        hint_.setFillColor(sf::Color(160, 180, 215));
        hint_.setPosition(425.0F, 150.0F);

        hud_.setFont(font_);
        hud_.setCharacterSize(19);
        hud_.setFillColor(sf::Color(225, 228, 238));
        hud_.setPosition(180.0F, 24.0F);

        shopTitle_.setFont(font_);
        shopTitle_.setCharacterSize(26);
        shopTitle_.setFillColor(sf::Color(235, 238, 248));
        shopTitle_.setPosition(880.0F, 72.0F);

        reserveTitle_.setFont(font_);
        reserveTitle_.setString(L"备用区");
        reserveTitle_.setCharacterSize(18);
        reserveTitle_.setFillColor(sf::Color(220, 225, 238));
        reserveTitle_.setPosition(20.0F, 590.0F);

        message_.setFont(font_);
        message_.setCharacterSize(22);
        message_.setPosition(390.0F, 625.0F);
    }

    // 此函数把一次选择按钮点击转换为待提交核心命令。
    void MatchScreen::handleEvent(const sf::Event& event)
    {
        // 此代码块在准备阶段优先处理单位拖拽的按下、移动和释放。
        if (view_.phase == core::MatchPhase::Preparation)
        {
            if (event.type == sf::Event::MouseButtonPressed
                && event.mouseButton.button == sf::Mouse::Left)
            {
                const sf::Vector2f pixel(
                    static_cast<float>(event.mouseButton.x),
                    static_cast<float>(event.mouseButton.y));
                const core::OwnedUnitId unitId = hitTestOwnedUnit(pixel);
                if (unitId != core::InvalidOwnedUnitId)
                {
                    drag_.active = true;
                    drag_.unitId = unitId;
                    drag_.mousePosition = pixel;
                    return;
                }
            }
            if (event.type == sf::Event::MouseMoved && drag_.active)
            {
                drag_.mousePosition = sf::Vector2f(
                    static_cast<float>(event.mouseMove.x),
                    static_cast<float>(event.mouseMove.y));
                return;
            }
            if (event.type == sf::Event::MouseButtonReleased
                && event.mouseButton.button == sf::Mouse::Left
                && drag_.active)
            {
                finishBasicDrag(sf::Vector2f(
                    static_cast<float>(event.mouseButton.x),
                    static_cast<float>(event.mouseButton.y)));
                return;
            }
        }

        // 此代码块让路线调试开关只在已经选定地图时可用。
        if (routeToggleButton_ != nullptr
            && routeToggleButton_->handleEvent(event))
        {
            showRoutes_ = !showRoutes_;
            routeToggleButton_->setLabel(
                showRoutes_ ? L"隐藏路线" : L"显示路线");
        }

        // 此代码块把准备阶段商店卡片点击转换为对应槽位购买命令。
        for (const ShopCard& card : shopCards_)
        {
            if (card.button != nullptr && card.button->handleEvent(event))
            {
                pendingAction_.kind = UiActionKind::SubmitCommand;
                pendingAction_.command = core::GameCommand{
                    core::MapSide::A,
                    core::PurchaseUnitCommand{card.slot}};
                return;
            }
        }

        // 此代码块把刷新按钮点击转换为无参数刷新命令。
        if (refreshButton_ != nullptr && refreshButton_->handleEvent(event))
        {
            pendingAction_.kind = UiActionKind::SubmitCommand;
            pendingAction_.command = core::GameCommand{
                core::MapSide::A,
                core::RefreshShopCommand{}};
            return;
        }

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
        // 此代码块在选定地图后的所有阶段绘制棋盘和路线开关。
        if (view_.selectedMap.has_value() && boardTransform_.valid())
        {
            MapRenderer::draw(
                target,
                view_.selectedMap.value(),
                boardTransform_,
                showRoutes_);
            if (routeToggleButton_ != nullptr)
            {
                routeToggleButton_->draw(target);
            }
        }

        // 此代码块在准备阶段绘制经济 HUD、商店标题、六槽和刷新按钮。
        if (view_.phase == core::MatchPhase::Preparation)
        {
            target.draw(hud_);
            target.draw(shopTitle_);
            for (const ShopCard& card : shopCards_)
            {
                if (card.button != nullptr)
                {
                    card.button->draw(target);
                }
            }
            if (refreshButton_ != nullptr)
            {
                refreshButton_->draw(target);
            }
            drawPreparationUnits(target);
        }

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
        // 此代码块让最近命令结果显示三秒后自动隐藏。
        if (!message_.getString().isEmpty()
            && messageClock_.getElapsedTime().asSeconds() < 3.0F)
        {
            target.draw(message_);
        }
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
        // 此代码块在地图存在时为固定棋盘视口更新双向坐标变换。
        if (view_.selectedMap.has_value())
        {
            boardTransform_.configure(
                sf::FloatRect(20.0F, 88.0F, 840.0F, 504.0F),
                view_.selectedMap->width,
                view_.selectedMap->height);
        }
        refreshPreparationWidgets();
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
        messageClock_.restart();
    }

    // 此函数根据核心选择列表建立动态数量的选择按钮。
    void MatchScreen::rebuildChoices()
    {
        choices_.clear();
        shopCards_.clear();
        routeToggleButton_.reset();
        refreshButton_.reset();
        message_.setString(L"");

        title_.setPosition(470.0F, 80.0F);
        hint_.setPosition(425.0F, 150.0F);
        message_.setPosition(390.0F, 625.0F);

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
            hint_.setString(L"蓝色为己方部署格，红色为电脑部署格");
            title_.setPosition(20.0F, 18.0F);
            hint_.setPosition(220.0F, 28.0F);
            message_.setPosition(890.0F, 650.0F);
            routeToggleButton_ = std::make_unique<Button>(
                font_,
                sf::FloatRect(690.0F, 18.0F, 170.0F, 46.0F),
                L"显示路线",
                20);

            // 此代码块为正式六槽商店建立两列三行的稳定按钮布局。
            if (view_.selfShop.has_value())
            {
                for (std::size_t slot = 0;
                     slot < view_.selfShop->offers.size();
                     ++slot)
                {
                    const std::size_t column = slot % 2;
                    const std::size_t row = slot / 2;
                    ShopCard card;
                    card.slot = slot;
                    card.button = std::make_unique<Button>(
                        font_,
                        sf::FloatRect(
                            880.0F + 190.0F * static_cast<float>(column),
                            112.0F + 102.0F * static_cast<float>(row),
                            178.0F,
                            86.0F),
                        L"商品",
                        19);
                    shopCards_.push_back(std::move(card));
                }
            }

            refreshButton_ = std::make_unique<Button>(
                font_,
                sf::FloatRect(880.0F, 430.0F, 368.0F, 52.0F),
                L"刷新商店",
                21);
            refreshPreparationWidgets();
        }
    }

    // 此函数从只读快照生成准备 HUD 并同步全部商店按钮。
    void MatchScreen::refreshPreparationWidgets()
    {
        // 此代码块在非准备阶段或缺少玩家数据时保持控件为空。
        if (view_.phase != core::MatchPhase::Preparation
            || !view_.self.has_value()
            || !view_.selfShop.has_value())
        {
            hud_.setString(L"");
            shopTitle_.setString(L"");
            return;
        }

        const core::PlayerState& player = view_.self.value();
        const int opponentGuard = view_.opponent.available
            ? view_.opponent.guardValue
            : 0;
        const std::uint64_t secondsRemaining =
            (view_.preparationFramesRemaining + 59U) / 60U;
        std::wostringstream hudStream;
        hudStream << L"第 " << view_.currentRound << L" / "
                  << view_.maxRounds << L" 回合    金币 " << player.gold
                  << L"    我方守卫 " << player.guardValue
                  << L"    敌方守卫 " << opponentGuard
                  << L"    剩余 " << secondsRemaining << L" 秒";
        hud_.setString(hudStream.str());
        shopTitle_.setString(L"商店");

        const core::ShopState& shop = view_.selfShop.value();
        // 此代码块逐槽显示核心价格并禁用已经购买的空商品。
        for (ShopCard& card : shopCards_)
        {
            if (card.button == nullptr || card.slot >= shop.offers.size())
            {
                continue;
            }
            const std::optional<core::ShopOffer>& offer =
                shop.offers[card.slot];
            if (!offer.has_value())
            {
                card.button->setLabel(L"已购买");
                card.button->setEnabled(false);
                continue;
            }

            sf::String label = unitName(offer->unitId);
            label += L"  Lv.1\n";
            label += sf::String(std::to_wstring(offer->displayedPrice));
            label += L" 金币";
            card.button->setLabel(label);
            card.button->setEnabled(true);
        }

        // 此代码块让刷新按钮显示正式配置中的费用但仍允许核心报告金币不足。
        if (refreshButton_ != nullptr)
        {
            sf::String label = L"刷新商店  ";
            label += sf::String(std::to_wstring(
                config_.gameConfig.shopRefreshCost));
            label += L" 金币";
            refreshButton_->setLabel(label);
            refreshButton_->setEnabled(true);
        }
    }

    // 此函数从已校验配置查找单位中文名且缺失时显示稳定 ID。
    sf::String MatchScreen::unitName(const std::string& unitId) const
    {
        // 此代码块线性查找数量固定为五个的单位定义集合。
        for (const core::UnitDefinition& definition : config_.units)
        {
            if (definition.id == unitId)
            {
                return fromUtf8(definition.name);
            }
        }
        return fromUtf8(unitId);
    }

    // 此函数在棋盘下方为八个正式备用槽分配等宽矩形。
    sf::FloatRect MatchScreen::reserveSlotBounds(const std::size_t slot) noexcept
    {
        return sf::FloatRect(
            20.0F + 105.0F * static_cast<float>(slot),
            610.0F,
            98.0F,
            88.0F);
    }

    // 此函数先检测备用槽，再检测己方部署格中的活动单位。
    core::OwnedUnitId MatchScreen::hitTestOwnedUnit(
        const sf::Vector2f pixel) const noexcept
    {
        // 此代码块在玩家快照不存在时拒绝开始拖拽。
        if (!view_.self.has_value())
        {
            return core::InvalidOwnedUnitId;
        }

        const core::PlayerState& player = view_.self.value();
        // 此代码块按照备用槽视觉顺序查找命中的己方单位。
        for (std::size_t slot = 0; slot < player.reserveSlots.size(); ++slot)
        {
            if (player.reserveSlots[slot].has_value()
                && reserveSlotBounds(slot).contains(pixel))
            {
                return player.reserveSlots[slot].value();
            }
        }

        // 此代码块按照部署映射顺序查找鼠标所在己方单位格。
        for (const auto& deployment : player.deployments)
        {
            if (boardTransform_.cellBounds(deployment.first).contains(pixel))
            {
                return deployment.second;
            }
        }
        return core::InvalidOwnedUnitId;
    }

    // 此函数把拖拽释放转换为地图部署、备用槽移动或本地失败提示。
    void MatchScreen::finishBasicDrag(const sf::Vector2f pixel)
    {
        const core::OwnedUnitId unitId = drag_.unitId;
        drag_ = DragState{};

        // 此代码块优先把棋盘内释放转换为部署命令并让核心验证格子是否合法。
        if (const std::optional<core::GridPosition> grid =
                boardTransform_.pixelToGrid(pixel))
        {
            pendingAction_.kind = UiActionKind::SubmitCommand;
            pendingAction_.command = core::GameCommand{
                core::MapSide::A,
                core::MoveToDeploymentCommand{unitId, grid.value()}};
            return;
        }

        // 此代码块把备用区内释放转换为指定槽位的返回命令。
        if (view_.self.has_value())
        {
            for (std::size_t slot = 0;
                 slot < view_.self->reserveSlots.size();
                 ++slot)
            {
                if (reserveSlotBounds(slot).contains(pixel))
                {
                    pendingAction_.kind = UiActionKind::SubmitCommand;
                    pendingAction_.command = core::GameCommand{
                        core::MapSide::A,
                        core::MoveToReserveCommand{unitId, slot}};
                    return;
                }
            }
        }

        showLocalMessage(L"请拖到地图部署格或备用槽", false);
    }

    // 此函数线性查找最多八个己方活动单位并返回稳定快照地址。
    const core::OwnedUnit* MatchScreen::findActiveUnit(
        const core::OwnedUnitId unitId) const noexcept
    {
        // 此代码块在玩家快照不存在时返回空指针。
        if (!view_.self.has_value())
        {
            return nullptr;
        }
        for (const core::OwnedUnit& unit : view_.self->activeUnits)
        {
            if (unit.id == unitId)
            {
                return &unit;
            }
        }
        return nullptr;
    }

    // 此函数绘制备用槽、双方部署单位以及当前拖拽跟随标记。
    void MatchScreen::drawPreparationUnits(sf::RenderTarget& target) const
    {
        // 此代码块在玩家快照缺失时不绘制任何准备单位内容。
        if (!view_.self.has_value())
        {
            return;
        }

        target.draw(reserveTitle_);
        const core::PlayerState& player = view_.self.value();
        // 此代码块绘制每个备用槽及其中未被拖拽的单位。
        for (std::size_t slot = 0; slot < player.reserveSlots.size(); ++slot)
        {
            const sf::FloatRect bounds = reserveSlotBounds(slot);
            sf::RectangleShape slotShape(
                sf::Vector2f(bounds.width, bounds.height));
            slotShape.setPosition(bounds.left, bounds.top);
            slotShape.setFillColor(sf::Color(42, 47, 60));
            slotShape.setOutlineThickness(2.0F);
            slotShape.setOutlineColor(sf::Color(105, 115, 140));
            target.draw(slotShape);

            if (!player.reserveSlots[slot].has_value()
                || player.reserveSlots[slot].value() == drag_.unitId)
            {
                continue;
            }
            const core::OwnedUnit* unit = findActiveUnit(
                player.reserveSlots[slot].value());
            if (unit != nullptr)
            {
                UnitRenderer::draw(
                    target,
                    font_,
                    unitName(unit->identity.unitId),
                    unit->identity,
                    core::MapSide::A,
                    bounds);
            }
        }

        // 此代码块绘制己方部署单位并在拖拽时隐藏原位置。
        for (const auto& deployment : player.deployments)
        {
            if (deployment.second == drag_.unitId)
            {
                continue;
            }
            const core::OwnedUnit* unit = findActiveUnit(deployment.second);
            if (unit != nullptr)
            {
                UnitRenderer::draw(
                    target,
                    font_,
                    unitName(unit->identity.unitId),
                    unit->identity,
                    core::MapSide::A,
                    boardTransform_.cellBounds(deployment.first));
            }
        }

        // 此代码块绘制只公开身份和位置的电脑部署单位。
        for (const core::PublicDeployedUnitView& unit
             : view_.opponent.deployments)
        {
            UnitRenderer::draw(
                target,
                font_,
                unitName(unit.identity.unitId),
                unit.identity,
                core::MapSide::B,
                boardTransform_.cellBounds(unit.position));
        }

        // 此代码块在拖拽期间绘制跟随鼠标的半透明单位标记。
        if (drag_.active)
        {
            const core::OwnedUnit* unit = findActiveUnit(drag_.unitId);
            if (unit != nullptr)
            {
                const sf::FloatRect ghostBounds(
                    drag_.mousePosition.x - 48.0F,
                    drag_.mousePosition.y - 40.0F,
                    96.0F,
                    80.0F);
                UnitRenderer::draw(
                    target,
                    font_,
                    unitName(unit->identity.unitId),
                    unit->identity,
                    core::MapSide::A,
                    ghostBounds,
                    180);
            }
        }
    }

    // 此函数显示界面层成功或失败消息并重置三秒计时。
    void MatchScreen::showLocalMessage(
        const sf::String& text,
        const bool success)
    {
        message_.setString(text);
        message_.setFillColor(
            success
                ? sf::Color(100, 215, 135)
                : sf::Color(240, 105, 105));
        messageClock_.restart();
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
