#include "game/screens/MatchScreen.hpp"

#include "game/rendering/MapRenderer.hpp"
#include "game/rendering/UnitRenderer.hpp"

#include <SFML/Graphics/VertexArray.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <utility>
#include <iostream>

namespace autochess::game
{
    namespace
    {
        constexpr float UnitAnimationHeightRatio = 1.36F;
        constexpr float UnitAnimationVerticalOffsetRatio = 0.20F;
        constexpr float TrainingGuardHorizontalOffsetRatio = 0.40F;
        constexpr float TrainingGuardAttackHorizontalOffsetRatio = 1.00F;
        constexpr float MedicAttackHorizontalOffsetRatio = 0.10F;
        constexpr double BattlePositionChangeEpsilon = 1.0e-6;

        // 此函数将角色可见中心上移，使脚底落在格子下部而不是越过格线。
        // 铁卫素材的装备轮廓左右不对称，需要按朝向反向补偿人物主体中心。
        sf::Vector2f unitAnimationPosition(
            const sf::Vector2f cellCenter,
            const float cellSize,
            const std::string& unitId,
            const bool faceRight,
            const UnitAnimationAction action = UnitAnimationAction::Relax)
        {
            sf::Vector2f position = cellCenter;
            position.y -= cellSize * UnitAnimationVerticalOffsetRatio;
            if (unitId == "training_guard")
            {
                const float direction = faceRight ? -1.0F : 1.0F;
                position.x += cellSize
                    * TrainingGuardHorizontalOffsetRatio
                    * direction;
            }

            // 攻击骨骼与准备骨骼的主体中心不同，只在攻击状态追加补偿。
            // AI 侧按朝向镜像，确保双方单位保持相同的相对位移。
            if (action == UnitAnimationAction::Attack)
            {
                float attackOffsetRatio = 0.0F;
                if (unitId == "training_guard")
                {
                    attackOffsetRatio = TrainingGuardAttackHorizontalOffsetRatio;
                }
                else if (unitId == "medic")
                {
                    attackOffsetRatio = MedicAttackHorizontalOffsetRatio;
                }

                const float direction = faceRight ? 1.0F : -1.0F;
                position.x += cellSize * attackOffsetRatio * direction;
            }
            return position;
        }

        // 此函数用容差判断连续战斗坐标是否真的发生了可见移动，避免
        // 浮点微小误差反复打断攻击动作。
        bool battlePositionChanged(
            const core::BattlePosition& previous,
            const core::BattlePosition& current) noexcept
        {
            return std::abs(previous.x - current.x)
                    > BattlePositionChangeEpsilon
                || std::abs(previous.y - current.y)
                    > BattlePositionChangeEpsilon;
        }

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
        const core::ConfigBundle& config,
        std::filesystem::path dataDirectory)
        : font_(font),
          config_(config),
          animationRepository_(std::move(dataDirectory))
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

        combatHud_.setFont(font_);
        combatHud_.setCharacterSize(20);
        combatHud_.setFillColor(sf::Color(225, 228, 238));
        combatHud_.setPosition(880.0F, 88.0F);

        selectedHud_.setFont(font_);
        selectedHud_.setCharacterSize(18);
        selectedHud_.setFillColor(sf::Color(205, 215, 235));
        selectedHud_.setPosition(880.0F, 190.0F);

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
        // 此代码块在暂停覆盖层开启时只允许继续、重开或返回菜单。
        if (paused_)
        {
            if (resumeButton_ != nullptr
                && resumeButton_->handleEvent(event))
            {
                pendingAction_.kind = UiActionKind::TogglePause;
                return;
            }
            if (restartButton_ != nullptr
                && restartButton_->handleEvent(event))
            {
                pendingAction_.kind = UiActionKind::RestartMatch;
                return;
            }
            if (resultMenuButton_ != nullptr
                && resultMenuButton_->handleEvent(event))
            {
                pendingAction_.kind = UiActionKind::BackToMenu;
                return;
            }
            return;
        }

        // 此代码块处理最终结果页的重新选择和主菜单导航动作。
        if (view_.phase == core::MatchPhase::MatchResult)
        {
            if (playAgainButton_ != nullptr
                && playAgainButton_->handleEvent(event))
            {
                pendingAction_.kind = UiActionKind::PlayAgain;
                return;
            }
            if (resultMenuButton_ != nullptr
                && resultMenuButton_->handleEvent(event))
            {
                pendingAction_.kind = UiActionKind::BackToMenu;
                return;
            }
            return;
        }

        // 此代码块让准备和战斗页面的暂停按钮优先于其他操作生效。
        if (pauseButton_ != nullptr && pauseButton_->handleEvent(event))
        {
            pendingAction_.kind = UiActionKind::TogglePause;
            return;
        }

        // 此代码块优先处理战斗选中和技能按钮，统一转换为核心命令。
        if (view_.phase == core::MatchPhase::Combat)
        {
            if (skillButton_ != nullptr
                && skillButton_->handleEvent(event))
            {
                // 此代码块在提交技能命令后锁定按钮直到核心返回结果。
                skillCommandPending_ = true;
                pendingAction_.kind = UiActionKind::SubmitCommand;
                pendingAction_.command = core::GameCommand{
                    core::MapSide::A,
                    core::ReleaseSkillCommand{selectedBattleUnitId_}};
                return;
            }
            if (event.type == sf::Event::MouseButtonPressed
                && event.mouseButton.button == sf::Mouse::Left)
            {
                const sf::Vector2f pixel(
                    static_cast<float>(event.mouseButton.x),
                    static_cast<float>(event.mouseButton.y));
                selectedBattleUnitId_ = hitTestBattleUnit(pixel);
                refreshCombatWidgets();
                return;
            }
        }

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
                    deploymentPreview_.reset();
                    return;
                }
            }
            if (event.type == sf::Event::MouseMoved && drag_.active)
            {
                drag_.mousePosition = sf::Vector2f(
                    static_cast<float>(event.mouseMove.x),
                    static_cast<float>(event.mouseMove.y));
                updateDeploymentPreview(drag_.mousePosition);
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

        // 此代码块让两个页签只切换右侧列表而不触碰核心状态。
        if (shopTabButton_ != nullptr && shopTabButton_->handleEvent(event))
        {
            showDeathList_ = false;
            refreshPreparationWidgets();
            return;
        }
        if (deathTabButton_ != nullptr && deathTabButton_->handleEvent(event))
        {
            showDeathList_ = true;
            refreshPreparationWidgets();
            return;
        }

        // 此代码块只处理当前可见页签中的购买或复活按钮。
        if (showDeathList_)
        {
            for (const ReviveCard& card : reviveCards_)
            {
                if (card.button != nullptr
                    && card.button->handleEvent(event))
                {
                    pendingAction_.kind = UiActionKind::SubmitCommand;
                    pendingAction_.command = core::GameCommand{
                        core::MapSide::A,
                        core::ReviveUnitCommand{card.unitId}};
                    return;
                }
            }
        }
        else
        {
            for (const ShopCard& card : shopCards_)
            {
                if (card.button != nullptr
                    && card.button->handleEvent(event))
                {
                    pendingAction_.kind = UiActionKind::SubmitCommand;
                    pendingAction_.command = core::GameCommand{
                        core::MapSide::A,
                        core::PurchaseUnitCommand{card.slot}};
                    return;
                }
            }
        }

        // 此代码块只在商店页签把刷新按钮点击转换为刷新命令。
        if (!showDeathList_
            && refreshButton_ != nullptr
            && refreshButton_->handleEvent(event))
        {
            pendingAction_.kind = UiActionKind::SubmitCommand;
            pendingAction_.command = core::GameCommand{
                core::MapSide::A,
                core::RefreshShopCommand{}};
            return;
        }

        // 此代码块把可用的提前开始按钮转换为统一开战命令。
        if (startButton_ != nullptr && startButton_->handleEvent(event))
        {
            pendingAction_.kind = UiActionKind::SubmitCommand;
            pendingAction_.command = core::GameCommand{
                core::MapSide::A,
                core::StartCombatCommand{}};
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
        // 此代码块只在准备、战斗和回合结算阶段绘制已经选定的棋盘。
        if (view_.selectedMap.has_value()
            && boardTransform_.valid()
            && (view_.phase == core::MatchPhase::Preparation
                || view_.phase == core::MatchPhase::Combat
                || view_.phase == core::MatchPhase::RoundSettlement))
        {
            MapRenderer::draw(
                target,
                view_.selectedMap.value(),
                boardTransform_,
                showRoutes_,
                deploymentPreview_);
            if (routeToggleButton_ != nullptr)
            {
                routeToggleButton_->draw(target);
            }
        }

        // 此代码块在准备阶段绘制经济 HUD、页签、当前列表和操作按钮。
        if (view_.phase == core::MatchPhase::Preparation)
        {
            target.draw(hud_);
            if (shopTabButton_ != nullptr)
            {
                shopTabButton_->draw(target);
            }
            if (deathTabButton_ != nullptr)
            {
                deathTabButton_->draw(target);
            }

            // 此代码块根据当前页签只绘制商店卡片或复活按钮。
            if (showDeathList_)
            {
                for (const ReviveCard& card : reviveCards_)
                {
                    if (card.button != nullptr)
                    {
                        card.button->draw(target);
                    }
                }
            }
            else
            {
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
            }
            drawSellZone(target);
            if (startButton_ != nullptr)
            {
                startButton_->draw(target);
            }
            drawPreparationUnits(target);
        }

        // 此代码块绘制战斗和准备阶段右上角的暂停入口。
        if (!paused_
            && (view_.phase == core::MatchPhase::Preparation
                || view_.phase == core::MatchPhase::Combat)
            && pauseButton_ != nullptr)
        {
            pauseButton_->draw(target);
        }

        // 此代码块在战斗及结算阶段叠加连续单位和战斗状态面板。
        if (view_.phase == core::MatchPhase::Combat
            || view_.phase == core::MatchPhase::RoundSettlement)
        {
            drawCombatUnits(target);
        }
        if (view_.phase == core::MatchPhase::Combat)
        {
            target.draw(combatHud_);
            target.draw(selectedHud_);
            if (skillButton_ != nullptr)
            {
                skillButton_->draw(target);
            }
        }

        // 此代码块在最终结果页绘制再来一局和返回主菜单按钮。
        if (view_.phase == core::MatchPhase::MatchResult)
        {
            if (playAgainButton_ != nullptr)
            {
                playAgainButton_->draw(target);
            }
            if (resultMenuButton_ != nullptr)
            {
                resultMenuButton_->draw(target);
            }
        }

        // 此代码块在暂停时覆盖底层画面并绘制三个应用层操作按钮。
        if (paused_)
        {
            sf::RectangleShape overlay(sf::Vector2f(1280.0F, 720.0F));
            overlay.setFillColor(sf::Color(15, 18, 25, 190));
            target.draw(overlay);
            sf::Text pausedText(L"游戏已暂停", font_, 42);
            pausedText.setFillColor(sf::Color(240, 242, 250));
            pausedText.setPosition(535.0F, 145.0F);
            target.draw(pausedText);
            if (resumeButton_ != nullptr)
            {
                resumeButton_->draw(target);
            }
            if (restartButton_ != nullptr)
            {
                restartButton_->draw(target);
            }
            if (resultMenuButton_ != nullptr)
            {
                resultMenuButton_->draw(target);
            }
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
        const core::MatchPhase previousPhase = view_.phase;
        view_ = view;
        syncAnimations();
        // 此代码块在离开准备阶段或阶段发生变化时清除瞬时拖拽预览。
        if (view_.phase != core::MatchPhase::Preparation
            || previousPhase != view_.phase)
        {
            drag_ = DragState{};
            deploymentPreview_.reset();
        }
        // 此代码块在地图存在时为固定棋盘视口更新双向坐标变换。
        if (view_.selectedMap.has_value())
        {
            boardTransform_.configure(
                sf::FloatRect(20.0F, 88.0F, 840.0F, 504.0F),
                view_.selectedMap->width,
                view_.selectedMap->height);
        }
        refreshPreparationWidgets();
        // 此代码块清理跨阶段失效的单位选择并同步战斗控件。
        if (view_.phase != core::MatchPhase::Combat)
        {
            selectedBattleUnitId_ = core::InvalidBattleUnitId;
        }
        refreshCombatWidgets();
        // 此代码块避免同一阶段每个渲染帧重复分配按钮。
        if (builtPhase_ != view_.phase)
        {
            builtPhase_ = view_.phase;
            rebuildChoices();
        }
    }

    std::size_t MatchScreen::AnimationUnitKeyHash::operator()(
        const AnimationUnitKey& key) const noexcept
    {
        const std::size_t idHash = std::hash<core::OwnedUnitId>{}(key.unitId);
        const std::size_t sideHash = std::hash<int>{}(
            static_cast<int>(key.side));
        return idHash ^ (sideHash + 0x9e3779b9U + (idHash << 6U)
                         + (idHash >> 2U));
    }

    UnitAnimationInstance* MatchScreen::animationFor(
        const core::MapSide side,
        const core::OwnedUnitId unitId,
        const core::UnitIdentity& identity)
    {
        if (side == core::MapSide::Unknown
            || unitId == core::InvalidOwnedUnitId
            || identity.unitId.empty())
        {
            return nullptr;
        }
        const AnimationUnitKey key{side, unitId};
        const auto existing = animations_.find(key);
        if (existing != animations_.end())
        {
            // 同一显示键若对应了新的单位种类，旧 Spine 实例绝不能复用，
            // 否则例如决斗者会继续显示先前缓存的奥术师模型。
            if (existing->second != nullptr
                && existing->second->unitId() == identity.unitId)
            {
                return existing->second.get();
            }
            discardAnimation(key);
        }

        std::string errorMessage;
        const UnitAnimationAssetPtr asset = animationRepository_.load(
            identity.unitId,
            errorMessage);
        if (asset == nullptr)
        {
            std::cerr << "[SPINE] " << errorMessage << '\n';
        }
        auto instance = std::make_unique<UnitAnimationInstance>(asset);
        UnitAnimationInstance* result = instance.get();
        animations_.emplace(key, std::move(instance));
        return result;
    }

    void MatchScreen::discardAnimation(
        const AnimationUnitKey& key) noexcept
    {
        animations_.erase(key);
        lastActionSequences_.erase(key);
        lastBattlePositions_.erase(key);
        deathAnimationsFinished_.erase(key);
    }

    void MatchScreen::updateAnimations(const float deltaSeconds)
    {
        for (const auto& entry : animations_)
        {
            if (entry.second != nullptr && entry.second->valid())
            {
                entry.second->update(deltaSeconds);
            }
        }
    }

    void MatchScreen::syncAnimations()
    {
        if (view_.phase == core::MatchPhase::Preparation)
        {
            animationCombatActive_ = false;
            lastActionSequences_.clear();
            lastBattlePositions_.clear();
            deathAnimationsFinished_.clear();
            if (view_.self.has_value())
            {
                for (const core::OwnedUnit& unit : view_.self->activeUnits)
                {
                    if (auto* animation = animationFor(
                            unit.ownerSide,
                            unit.id,
                            unit.identity))
                    {
                        animation->play(UnitAnimationAction::Relax);
                    }
                }
            }
            for (const auto& unit : view_.opponent.deployments)
            {
                if (auto* animation = animationFor(
                        view_.opponent.side,
                        unit.id,
                        unit.identity))
                {
                    animation->play(UnitAnimationAction::Relax);
                }
            }
            return;
        }

        if (view_.phase != core::MatchPhase::Combat
            && view_.phase != core::MatchPhase::RoundSettlement)
        {
            return;
        }

        // 回合结算仍需继续同步最后一批死亡动画；进入结算时不能清空状态。
        if (!animationCombatActive_)
        {
            animationCombatActive_ = true;
            lastActionSequences_.clear();
            lastBattlePositions_.clear();
            deathAnimationsFinished_.clear();
        }

        for (const core::BattleUnit& unit : view_.battleUnits)
        {
            const core::OwnedUnitId animationId =
                unit.ownedUnitId != core::InvalidOwnedUnitId
                ? unit.ownedUnitId
                : static_cast<core::OwnedUnitId>(unit.id);
            const AnimationUnitKey animationKey{unit.side, animationId};

            // 抵达守卫点的单位已不再参与战斗和绘制，立即释放实例及
            // 跨帧状态，避免它继续在后台更新或污染后续同键单位。
            if (unit.state == core::BattleUnitState::ReachedGuard)
            {
                discardAnimation(animationKey);
                continue;
            }

            UnitAnimationInstance* animation =
                animationFor(unit.side, animationId, unit.identity);

            // 死亡动画由每个单位独立推进。播放完成后立即标记为退场；
            // 素材缺失时直接退场，避免静态回退图永久留在棋盘上。
            if (unit.state == core::BattleUnitState::Dead)
            {
                bool& finished = deathAnimationsFinished_[animationKey];
                if (finished)
                {
                    continue;
                }
                if (animation == nullptr || !animation->valid())
                {
                    finished = true;
                    continue;
                }
                if (animation->currentAction() != UnitAnimationAction::Die)
                {
                    finished = !animation->play(UnitAnimationAction::Die, true);
                }
                else if (animation->currentAnimationComplete())
                {
                    finished = true;
                }
                continue;
            }

            if (animation == nullptr || !animation->valid())
            {
                continue;
            }
            const auto previousSequence = lastActionSequences_.find(animationKey);
            const bool newBasicAction = previousSequence != lastActionSequences_.end()
                && previousSequence->second != unit.basicActionSequence;
            const auto previousPosition = lastBattlePositions_.find(animationKey);
            const bool moved = previousPosition != lastBattlePositions_.end()
                && battlePositionChanged(
                    previousPosition->second,
                    unit.position);
            if (newBasicAction)
            {
                // 循环攻击已进入 Loop 后不应被每次伤害结算重新拉回
                // Begin；普通一次性攻击仍按 basicActionSequence 重播。
                if (!animation->repeatingAttack())
                {
                    animation->play(UnitAnimationAction::Attack, true);
                }
            }
            else if (moved)
            {
                // 核心已经恢复移动时立即切回 Move，不再等待较长的攻击
                // 序列结束，避免同帧击杀后多个存活角色一起视觉定格。
                animation->play(UnitAnimationAction::Move);
            }
            else if (animation->currentAction() == UnitAnimationAction::Attack
                     && !animation->currentAnimationComplete())
            {
                // 原地交战时仍完整播放攻击动作。
            }
            else if (animation->currentAction() != UnitAnimationAction::Move)
            {
                animation->play(UnitAnimationAction::Move);
            }
            lastActionSequences_[animationKey] = unit.basicActionSequence;
            lastBattlePositions_[animationKey] = unit.position;
        }
    }
    // 此函数使用颜色区分成功和失败的核心中文结果。
    void MatchScreen::showCommandResult(const core::CommandResult& result)
    {
        // 此代码块把核心结果作为技能队列的明确完成信号。
        skillCommandPending_ = false;
        message_.setString(fromUtf8(result.message));
        message_.setFillColor(
            result.success
                ? sf::Color(100, 215, 135)
                : sf::Color(240, 105, 105));
        messageClock_.restart();
    }

    // 此函数只记录应用层暂停状态，具体停止模拟由 GameApp 执行。
    void MatchScreen::setPaused(const bool paused) noexcept
    {
        paused_ = paused;
        // 此代码块在暂停开始时结束未完成拖拽，防止覆盖层下残留预览。
        if (paused_)
        {
            drag_ = DragState{};
            deploymentPreview_.reset();
        }
        refreshCombatWidgets();
    }

    // 此函数根据核心选择列表建立动态数量的选择按钮。
    void MatchScreen::rebuildChoices()
    {
        choices_.clear();
        shopCards_.clear();
        reviveCards_.clear();
        routeToggleButton_.reset();
        shopTabButton_.reset();
        deathTabButton_.reset();
        refreshButton_.reset();
        startButton_.reset();
        skillButton_.reset();
        pauseButton_.reset();
        resumeButton_.reset();
        restartButton_.reset();
        resultMenuButton_.reset();
        playAgainButton_.reset();
        showDeathList_ = false;
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
            hint_.setString(L"请选择电脑的决策风格：进攻、防守或路线");
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
        else if (view_.phase == core::MatchPhase::Preparation)
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

            // 此代码块建立商店和死亡列表两个互斥页签按钮。
            shopTabButton_ = std::make_unique<Button>(
                font_,
                sf::FloatRect(880.0F, 72.0F, 178.0F, 40.0F),
                L"【商店】",
                19);
            deathTabButton_ = std::make_unique<Button>(
                font_,
                sf::FloatRect(1070.0F, 72.0F, 178.0F, 40.0F),
                L"死亡列表 (0)",
                19);

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
                            122.0F + 96.0F * static_cast<float>(row),
                            178.0F,
                            80.0F),
                        L"商品",
                        19);
                    shopCards_.push_back(std::move(card));
                }
            }

            refreshButton_ = std::make_unique<Button>(
                font_,
                sf::FloatRect(880.0F, 420.0F, 368.0F, 52.0F),
                L"刷新商店",
                21);

            // 此代码块建立始终位于右侧底部的提前开战按钮。
            startButton_ = std::make_unique<Button>(
                font_,
                sf::FloatRect(880.0F, 570.0F, 368.0F, 52.0F),
                L"电脑正在准备",
                21);
            pauseButton_ = std::make_unique<Button>(
                font_,
                sf::FloatRect(1070.0F, 18.0F, 178.0F, 46.0F),
                L"暂停",
                20);
            resumeButton_ = std::make_unique<Button>(
                font_,
                sf::FloatRect(440.0F, 245.0F, 400.0F, 58.0F),
                L"继续游戏",
                22);
            restartButton_ = std::make_unique<Button>(
                font_,
                sf::FloatRect(440.0F, 325.0F, 400.0F, 58.0F),
                L"重新开始",
                22);
            resultMenuButton_ = std::make_unique<Button>(
                font_,
                sf::FloatRect(440.0F, 405.0F, 400.0F, 58.0F),
                L"返回主菜单",
                22);
            refreshPreparationWidgets();
        }
        else if (view_.phase == core::MatchPhase::Combat)
        {
            title_.setString(L"战斗进行中");
            hint_.setString(L"选择己方单位查看战斗状态");
            title_.setPosition(20.0F, 18.0F);
            hint_.setPosition(220.0F, 28.0F);
            message_.setPosition(890.0F, 650.0F);
            skillButton_ = std::make_unique<Button>(
                font_,
                sf::FloatRect(880.0F, 500.0F, 368.0F, 52.0F),
                L"技能未就绪",
                21);
            pauseButton_ = std::make_unique<Button>(
                font_,
                sf::FloatRect(1070.0F, 18.0F, 178.0F, 46.0F),
                L"暂停",
                20);
            resumeButton_ = std::make_unique<Button>(
                font_,
                sf::FloatRect(440.0F, 245.0F, 400.0F, 58.0F),
                L"继续游戏",
                22);
            restartButton_ = std::make_unique<Button>(
                font_,
                sf::FloatRect(440.0F, 325.0F, 400.0F, 58.0F),
                L"重新开始",
                22);
            resultMenuButton_ = std::make_unique<Button>(
                font_,
                sf::FloatRect(440.0F, 405.0F, 400.0F, 58.0F),
                L"返回主菜单",
                22);
            refreshCombatWidgets();
        }
        else if (view_.phase == core::MatchPhase::RoundSettlement)
        {
            title_.setString(L"回合结算");
            title_.setPosition(500.0F, 270.0F);
            hint_.setPosition(365.0F, 350.0F);
            if (view_.battleSummary.has_value())
            {
                const core::BattleSummary& summary =
                    view_.battleSummary.value();
                std::wostringstream settlementStream;
                settlementStream << L"战斗已结束，正在结算守卫伤害和单位状态\n"
                                  << L"对我方守卫伤害："
                                  << summary.guardDamageToA
                                  << L"    对电脑守卫伤害："
                                  << summary.guardDamageToB
                                  << L"\n死亡："
                                  << summary.deadOwnedUnitIds.size()
                                  << L"    到达守卫："
                                  << summary.reachedGuardOwnedUnitIds.size()
                                  << L"    存活："
                                  << summary.survivingOwnedUnitIds.size();
                hint_.setString(settlementStream.str());
            }
            else
            {
                hint_.setString(L"正在结算守卫伤害、单位状态和下一回合资源");
            }
        }
        else if (view_.phase == core::MatchPhase::MatchResult)
        {
            title_.setString(L"对局结束");
            title_.setPosition(535.0F, 220.0F);

            // 此代码块把核心最终结果转换为静态中文胜负和守卫摘要。
            sf::String outcome = L"对局结果未知";
            if (view_.result.outcome == core::MatchOutcome::SideAWin)
            {
                outcome = L"你获胜了";
            }
            else if (view_.result.outcome == core::MatchOutcome::SideBWin)
            {
                outcome = L"电脑获胜";
            }
            else if (view_.result.outcome == core::MatchOutcome::Draw)
            {
                outcome = L"本局平局";
            }

            std::wostringstream resultStream;
            resultStream << outcome.toWideString() << L"\n完成回合："
                         << view_.result.completedRounds
                         << L"    我方守卫：" << view_.result.guardValueA
                         << L"    敌方守卫：" << view_.result.guardValueB;
            hint_.setString(resultStream.str());
            hint_.setPosition(390.0F, 310.0F);
            playAgainButton_ = std::make_unique<Button>(
                font_,
                sf::FloatRect(390.0F, 470.0F, 240.0F, 58.0F),
                L"再来一局",
                21);
            resultMenuButton_ = std::make_unique<Button>(
                font_,
                sf::FloatRect(650.0F, 470.0F, 240.0F, 58.0F),
                L"返回主菜单",
                21);
        }
    }

    // 此函数从只读快照生成准备 HUD 并同步页签、商店和开战按钮。
    void MatchScreen::refreshPreparationWidgets()
    {
        // 此代码块在非准备阶段或缺少玩家数据时保持控件为空。
        if (view_.phase != core::MatchPhase::Preparation
            || !view_.self.has_value())
        {
            hud_.setString(L"");
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

        // 此代码块用书名号标明当前页签并实时显示死亡单位数量。
        if (shopTabButton_ != nullptr)
        {
            shopTabButton_->setLabel(
                showDeathList_ ? L"商店" : L"【商店】");
        }
        if (deathTabButton_ != nullptr)
        {
            sf::String label = showDeathList_ ? L"【死亡列表 (" : L"死亡列表 (";
            label += sf::String(std::to_wstring(player.deadUnits.size()));
            label += showDeathList_ ? L")】" : L")";
            deathTabButton_->setLabel(label);
        }
        syncReviveCards();

        // 此代码块在商店快照存在时逐槽显示核心价格和购买状态。
        if (view_.selfShop.has_value())
        {
            const core::ShopState& shop = view_.selfShop.value();
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

        // 此代码块在电脑尚无部署时禁用开战按钮并显示等待原因。
        if (startButton_ != nullptr)
        {
            const bool computerReady = view_.opponent.available
                && !view_.opponent.deployments.empty();
            startButton_->setLabel(
                computerReady ? L"提前开始战斗" : L"电脑正在准备");
            startButton_->setEnabled(computerReady);
        }
    }

    // 此函数比较稳定 ID 序列并只在死亡列表改变时重建复活按钮。
    void MatchScreen::syncReviveCards()
    {
        // 此代码块在玩家快照缺失时清空无法继续使用的死亡列表控件。
        if (!view_.self.has_value())
        {
            reviveCards_.clear();
            return;
        }

        const std::vector<core::OwnedUnit>& deadUnits =
            view_.self->deadUnits;
        const std::size_t visibleCount =
            std::min<std::size_t>(deadUnits.size(), 8U);
        bool needsRebuild = reviveCards_.size() != visibleCount;

        // 此代码块在数量相同的情况下继续比较每个死亡单位 ID。
        if (!needsRebuild)
        {
            for (std::size_t index = 0; index < visibleCount; ++index)
            {
                if (reviveCards_[index].unitId != deadUnits[index].id)
                {
                    needsRebuild = true;
                    break;
                }
            }
        }
        if (!needsRebuild)
        {
            return;
        }

        reviveCards_.clear();
        // 此代码块按两列四行建立最多八个稳定复活按钮。
        for (std::size_t index = 0; index < visibleCount; ++index)
        {
            const core::OwnedUnit& unit = deadUnits[index];
            const std::size_t column = index % 2;
            const std::size_t row = index / 2;
            sf::String label = unitName(unit.identity.unitId);
            label += L"  Lv.";
            label += sf::String(std::to_wstring(unit.identity.level));
            label += L"\n点击复活";

            ReviveCard card;
            card.unitId = unit.id;
            card.button = std::make_unique<Button>(
                font_,
                sf::FloatRect(
                    880.0F + 190.0F * static_cast<float>(column),
                    122.0F + 74.0F * static_cast<float>(row),
                    178.0F,
                    64.0F),
                label,
                18);
            reviveCards_.push_back(std::move(card));
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

    // 此函数在商店下方提供不会与棋盘或备用区重叠的出售区域。
    sf::FloatRect MatchScreen::sellZoneBounds() noexcept
    {
        return sf::FloatRect(880.0F, 500.0F, 368.0F, 58.0F);
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

    // 此函数只根据只读地图和玩家快照维护己方空部署格的悬停预览。
    void MatchScreen::updateDeploymentPreview(
        const sf::Vector2f pixel) noexcept
    {
        deploymentPreview_.reset();
        if (!drag_.active
            || !view_.selectedMap.has_value()
            || !view_.self.has_value())
        {
            return;
        }

        const std::optional<core::GridPosition> grid =
            boardTransform_.pixelToGrid(pixel);
        if (!grid.has_value())
        {
            return;
        }

        const core::MapDefinition& map = view_.selectedMap.value();
        const core::GridPosition target = grid.value();
        if (target.x < 0 || target.y < 0
            || target.x >= map.width || target.y >= map.height
            || map.gridRows.size() != static_cast<std::size_t>(map.height)
            || map.gridRows[static_cast<std::size_t>(target.y)].size()
                != static_cast<std::size_t>(map.width)
            || map.gridRows[static_cast<std::size_t>(target.y)]
                           [static_cast<std::size_t>(target.x)] != 'A')
        {
            return;
        }

        // 此代码块把已有单位的格子保留给合成交互，不显示普通部署预览。
        if (view_.self->deployments.find(target)
            != view_.self->deployments.end())
        {
            return;
        }

        const auto route = std::find_if(
            map.routes.begin(),
            map.routes.end(),
            [target](const core::Route& candidate) {
                return candidate.side == core::MapSide::A
                    && candidate.start == target;
            });
        if (route != map.routes.end())
        {
            deploymentPreview_ = target;
        }
    }

    // 此函数把拖拽释放转换为地图部署、备用槽移动或本地失败提示。
    void MatchScreen::finishBasicDrag(const sf::Vector2f pixel)
    {
        const core::OwnedUnitId unitId = drag_.unitId;
        drag_ = DragState{};
        deploymentPreview_.reset();

        // 此代码块让出售区优先于其他投放区域生成出售命令。
        if (sellZoneBounds().contains(pixel))
        {
            pendingAction_.kind = UiActionKind::SubmitCommand;
            pendingAction_.command = core::GameCommand{
                core::MapSide::A,
                core::SellUnitCommand{unitId}};
            return;
        }

        // 此代码块把棋盘内占用格转换为合成，其余格转换为部署命令。
        if (const std::optional<core::GridPosition> grid =
                boardTransform_.pixelToGrid(pixel))
        {
            // 此代码块在己方部署映射中查找目标格已存在的合成对象。
            if (view_.self.has_value())
            {
                const auto iterator = view_.self->deployments.find(grid.value());
                if (iterator != view_.self->deployments.end()
                    && iterator->second != unitId)
                {
                    pendingAction_.kind = UiActionKind::SubmitCommand;
                    pendingAction_.command = core::GameCommand{
                        core::MapSide::A,
                        core::MergeUnitsCommand{unitId, iterator->second}};
                    return;
                }
            }
            pendingAction_.kind = UiActionKind::SubmitCommand;
            pendingAction_.command = core::GameCommand{
                core::MapSide::A,
                core::MoveToDeploymentCommand{unitId, grid.value()}};
            return;
        }

        // 此代码块把占用备用槽转换为合成，把空槽转换为返回命令。
        if (view_.self.has_value())
        {
            for (std::size_t slot = 0;
                 slot < view_.self->reserveSlots.size();
                 ++slot)
            {
                if (reserveSlotBounds(slot).contains(pixel))
                {
                    const std::optional<core::OwnedUnitId>& target =
                        view_.self->reserveSlots[slot];
                    if (target.has_value() && target.value() != unitId)
                    {
                        pendingAction_.kind = UiActionKind::SubmitCommand;
                        pendingAction_.command = core::GameCommand{
                            core::MapSide::A,
                            core::MergeUnitsCommand{unitId, target.value()}};
                        return;
                    }
                    pendingAction_.kind = UiActionKind::SubmitCommand;
                    pendingAction_.command = core::GameCommand{
                        core::MapSide::A,
                        core::MoveToReserveCommand{unitId, slot}};
                    return;
                }
            }
        }

        showLocalMessage(L"请拖到部署格、备用槽或出售区", false);
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

    // 此函数根据当前快照同步战斗 HUD、选中信息和技能按钮状态。
    void MatchScreen::refreshCombatWidgets()
    {
        if (view_.phase != core::MatchPhase::Combat)
        {
            skillCommandPending_ = false;
            combatHud_.setString(L"");
            selectedHud_.setString(L"");
            if (skillButton_ != nullptr)
            {
                skillButton_->setEnabled(false);
            }
            return;
        }

        const int opponentGuard = view_.opponent.available
            ? view_.opponent.guardValue
            : 0;
        const std::uint64_t secondsRemaining =
            (view_.combatFramesRemaining + 59U) / 60U;
        std::wostringstream hudStream;
        hudStream << L"第 " << view_.currentRound << L" / "
                  << view_.maxRounds << L" 回合\n我方守卫 "
                  << (view_.self.has_value() ? view_.self->guardValue : 0)
                  << L"    敌方守卫 " << opponentGuard
                  << L"\n战斗剩余 " << secondsRemaining << L" 秒";
        combatHud_.setString(hudStream.str());

        const core::BattleUnit* selected = nullptr;
        for (const core::BattleUnit& unit : view_.battleUnits)
        {
            if (unit.id == selectedBattleUnitId_)
            {
                selected = &unit;
                break;
            }
        }

        bool skillReady = false;
        if (selected != nullptr
            && selected->side == core::MapSide::A
            && selected->state == core::BattleUnitState::Alive)
        {
            std::wostringstream selectedStream;
            selectedStream << unitName(selected->identity.unitId).toWideString()
                           << L"  Lv." << selected->identity.level
                           << L"\n生命 " << static_cast<int>(selected->health)
                           << L" / " << static_cast<int>(selected->stats.maxHealth)
                           << L"\n技力 " << static_cast<int>(selected->currentMana)
                           << L" / " << static_cast<int>(selected->maxMana);
            if (selected->targetId.has_value())
            {
                const auto target = std::find_if(
                    view_.battleUnits.begin(),
                    view_.battleUnits.end(),
                    [targetId = selected->targetId.value()](
                        const core::BattleUnit& unit) {
                        return unit.id == targetId;
                    });
                selectedStream << L"\n目标："
                               << (target == view_.battleUnits.end()
                                       ? sf::String(L"无").toWideString()
                                       : unitName(target->identity.unitId)
                                             .toWideString());
            }
            else
            {
                selectedStream << L"\n目标：无";
            }
            selectedHud_.setString(selectedStream.str());
            skillReady = selected->maxMana > 0.0
                && selected->currentMana >= selected->maxMana
                && !selected->activeSkill.active;
        }
        else
        {
            selectedHud_.setString(L"未选择己方单位");
        }

        if (skillButton_ != nullptr)
        {
            skillButton_->setLabel(
                skillReady ? L"释放技能" : L"技能未就绪");
            skillButton_->setEnabled(
                skillReady && !paused_ && !skillCommandPending_);
        }
    }

    // 此函数绘制目标连线和所有仍在战场上的实时单位。
    void MatchScreen::drawCombatUnits(sf::RenderTarget& target) const
    {
        if (!boardTransform_.valid())
        {
            return;
        }

        const float tileSize =
            boardTransform_.cellBounds({0, 0}).width;
        const float radius = std::clamp(tileSize * 0.28F, 10.0F, 18.0F);

        // 此代码块先绘制当前目标连线，确保连线位于单位标记下方。
        for (const core::BattleUnit& unit : view_.battleUnits)
        {
            if (unit.state != core::BattleUnitState::Alive
                || !unit.targetId.has_value())
            {
                continue;
            }
            const auto targetUnit = std::find_if(
                view_.battleUnits.begin(),
                view_.battleUnits.end(),
                [targetId = unit.targetId.value()](
                    const core::BattleUnit& candidate) {
                    return candidate.id == targetId
                        && candidate.state == core::BattleUnitState::Alive;
                });
            if (targetUnit == view_.battleUnits.end())
            {
                continue;
            }
            sf::VertexArray line(sf::Lines, 2);
            line[0].position =
                boardTransform_.battlePositionToPixel(unit.position);
            line[1].position =
                boardTransform_.battlePositionToPixel(targetUnit->position);
            line[0].color = sf::Color(235, 235, 220, 155);
            line[1].color = sf::Color(235, 235, 220, 155);
            target.draw(line);
        }

        // 此代码块绘制单位状态标记，并让死亡单位保留到死亡动画播放期间。
        for (const core::BattleUnit& unit : view_.battleUnits)
        {
            if (unit.state == core::BattleUnitState::ReachedGuard)
            {
                continue;
            }

            const core::OwnedUnitId animationId =
                unit.ownedUnitId != core::InvalidOwnedUnitId
                ? unit.ownedUnitId
                : static_cast<core::OwnedUnitId>(unit.id);
            const AnimationUnitKey animationKey{unit.side, animationId};
            const auto deathState = deathAnimationsFinished_.find(animationKey);
            if (unit.state == core::BattleUnitState::Dead
                && deathState != deathAnimationsFinished_.end()
                && deathState->second)
            {
                continue;
            }

            const sf::Vector2f center =
                boardTransform_.battlePositionToPixel(unit.position);
            UnitRenderer::drawBattle(
                target,
                font_,
                unit,
                center,
                radius,
                unit.id == selectedBattleUnitId_);

            // 此代码块优先绘制已同步的Spine角色，资源失败时保留上面的静态回退。
            const auto animationIt = animations_.find(animationKey);
            if (animationIt != animations_.end()
                && animationIt->second != nullptr
                && animationIt->second->valid())
            {
                const bool faceRight = unit.side == core::MapSide::A;
                animationIt->second->draw(
                    target,
                    unitAnimationPosition(
                        center,
                        tileSize,
                        unit.identity.unitId,
                        faceRight,
                        animationIt->second->currentAction()),
                    animationIt->second->scaleForHeight(tileSize * UnitAnimationHeightRatio),
                    faceRight);
            }
        }
    }

    // 此函数按绘制半径命中离鼠标最近的己方存活战斗单位。
    core::BattleUnitId MatchScreen::hitTestBattleUnit(
        const sf::Vector2f pixel) const noexcept
    {
        if (!boardTransform_.valid())
        {
            return core::InvalidBattleUnitId;
        }

        const float tileSize =
            boardTransform_.cellBounds({0, 0}).width;
        const float radius = std::clamp(tileSize * 0.28F, 10.0F, 18.0F);
        const float hitRadius = radius + 4.0F;
        const float hitRadiusSquared = hitRadius * hitRadius;
        core::BattleUnitId bestId = core::InvalidBattleUnitId;
        float bestDistanceSquared = std::numeric_limits<float>::max();
        for (const core::BattleUnit& unit : view_.battleUnits)
        {
            if (unit.side != core::MapSide::A
                || unit.state != core::BattleUnitState::Alive)
            {
                continue;
            }
            const sf::Vector2f center =
                boardTransform_.battlePositionToPixel(unit.position);
            const float dx = pixel.x - center.x;
            const float dy = pixel.y - center.y;
            const float distanceSquared = dx * dx + dy * dy;
            if (distanceSquared <= hitRadiusSquared
                && (distanceSquared < bestDistanceSquared
                    || (distanceSquared == bestDistanceSquared
                        && unit.id < bestId)))
            {
                bestId = unit.id;
                bestDistanceSquared = distanceSquared;
            }
        }
        return bestId;
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
                const AnimationUnitKey animationKey{
                    unit->ownerSide,
                    unit->id};
                const auto animationIt = animations_.find(animationKey);
                if (animationIt != animations_.end()
                    && animationIt->second != nullptr
                    && animationIt->second->valid())
                {
                    animationIt->second->draw(
                        target,
                        unitAnimationPosition(
                            sf::Vector2f(
                                bounds.left + bounds.width / 2.0F,
                                bounds.top + bounds.height / 2.0F),
                            bounds.height,
                            unit->identity.unitId,
                            true),
                        animationIt->second->scaleForHeight(bounds.height * UnitAnimationHeightRatio),
                        true);
                }
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
                const sf::FloatRect bounds =
                    boardTransform_.cellBounds(deployment.first);
                UnitRenderer::draw(
                    target,
                    font_,
                    unitName(unit->identity.unitId),
                    unit->identity,
                    core::MapSide::A,
                    bounds);
                const AnimationUnitKey animationKey{
                    unit->ownerSide,
                    unit->id};
                const auto animationIt = animations_.find(animationKey);
                if (animationIt != animations_.end()
                    && animationIt->second != nullptr
                    && animationIt->second->valid())
                {
                    animationIt->second->draw(
                        target,
                        unitAnimationPosition(
                            sf::Vector2f(
                                bounds.left + bounds.width / 2.0F,
                                bounds.top + bounds.height / 2.0F),
                            bounds.height,
                            unit->identity.unitId,
                            true),
                        animationIt->second->scaleForHeight(bounds.height * UnitAnimationHeightRatio),
                        true);
                }
            }
        }

        // 此代码块绘制只公开身份和位置的电脑部署单位。
        for (const core::PublicDeployedUnitView& unit
             : view_.opponent.deployments)
        {
            const sf::FloatRect bounds =
                boardTransform_.cellBounds(unit.position);
            UnitRenderer::draw(
                target,
                font_,
                unitName(unit.identity.unitId),
                unit.identity,
                core::MapSide::B,
                bounds);
            const AnimationUnitKey animationKey{
                view_.opponent.side,
                unit.id};
            const auto animationIt = animations_.find(animationKey);
            if (animationIt != animations_.end()
                && animationIt->second != nullptr
                && animationIt->second->valid())
            {
                animationIt->second->draw(
                    target,
                    unitAnimationPosition(
                        sf::Vector2f(
                            bounds.left + bounds.width / 2.0F,
                            bounds.top + bounds.height / 2.0F),
                        bounds.height,
                        unit.identity.unitId,
                        false),
                    animationIt->second->scaleForHeight(bounds.height * UnitAnimationHeightRatio),
                    false);
            }
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

    // 此函数绘制醒目的出售区域并提示拖入后会立即出售。
    void MatchScreen::drawSellZone(sf::RenderTarget& target) const
    {
        const sf::FloatRect bounds = sellZoneBounds();
        sf::RectangleShape zone(sf::Vector2f(bounds.width, bounds.height));
        zone.setPosition(bounds.left, bounds.top);
        zone.setFillColor(
            drag_.active
                ? sf::Color(115, 45, 50)
                : sf::Color(75, 42, 48));
        zone.setOutlineThickness(2.0F);
        zone.setOutlineColor(sf::Color(210, 85, 90));
        target.draw(zone);

        // 此代码块把出售提示精确居中到投放区域。
        sf::Text label(L"出售区（拖入出售）", font_, 20);
        label.setFillColor(sf::Color(250, 210, 210));
        const sf::FloatRect textBounds = label.getLocalBounds();
        label.setOrigin(
            textBounds.left + textBounds.width / 2.0F,
            textBounds.top + textBounds.height / 2.0F);
        label.setPosition(
            bounds.left + bounds.width / 2.0F,
            bounds.top + bounds.height / 2.0F);
        target.draw(label);
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
