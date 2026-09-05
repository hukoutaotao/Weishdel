#pragma once

#include "core/match/ReadOnlyGameView.hpp"
#include "core/config/ConfigBundleLoader.hpp"
#include "game/rendering/BoardTransform.hpp"
#include "game/animation/SpineAssetRepository.hpp"
#include "game/animation/UnitAnimationInstance.hpp"
#include "game/screens/Screen.hpp"
#include "game/ui/Button.hpp"

#include <SFML/Graphics/Text.hpp>
#include <SFML/System/Clock.hpp>

#include <memory>
#include <filesystem>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

namespace autochess::game
{
    // 此页面根据核心阶段显示地图、分队、AI 选择或准备占位内容。
    class MatchScreen final : public Screen
    {
    public:
        MatchScreen(
            const sf::Font& font,
            const sf::Font& englishFont,
            const sf::Font& guardFont,
            const core::ConfigBundle& config,
            std::filesystem::path dataDirectory);

        void handleEvent(const sf::Event& event) override;
        void draw(sf::RenderTarget& target) const override;
        UiAction takeAction() override;

        // 此函数复制最新只读快照并在阶段变化时重建选择按钮。
        void updateView(const core::ReadOnlyGameView& view);

        // 此函数以固定步长推进已加载的 Spine 实例，不改变核心结算。
        void updateAnimations(float deltaSeconds);

        // 此函数显示最近一次核心命令的中文结果。
        void showCommandResult(const core::CommandResult& result);

        // 此函数同步应用层暂停状态并阻止底层战斗控件响应。
        void setPaused(bool paused) noexcept;

        // 此函数同步应用层结算停留时间并刷新下一回合倒计时。
        void setSettlementCountdown(std::uint64_t framesRemaining);

    private:
        struct Choice
        {
            std::unique_ptr<Button> button;
            std::string id;
            sf::String displayName;
            core::AiStrategyKind strategy = core::AiStrategyKind::Unknown;
        };

        struct ShopCard
        {
            std::unique_ptr<Button> button;
            std::size_t slot = 0;
        };

        // 此结构把一个死亡单位 ID 与对应复活按钮稳定绑定。
        struct ReviveCard
        {
            std::unique_ptr<Button> button;
            core::OwnedUnitId unitId = core::InvalidOwnedUnitId;
        };

        struct DragState
        {
            bool active = false;
            core::OwnedUnitId unitId = core::InvalidOwnedUnitId;
            sf::Vector2f mousePosition;
        };

        // A、B 双方的 OwnedUnitId 会分别从 1 开始，必须把阵营纳入动画键，
        // 否则同编号的双方单位会错误共享同一个角色实例和动作状态。
        struct AnimationUnitKey
        {
            core::MapSide side = core::MapSide::Unknown;
            core::OwnedUnitId unitId = core::InvalidOwnedUnitId;

            bool operator==(const AnimationUnitKey& other) const noexcept
            {
                return side == other.side && unitId == other.unitId;
            }
        };

        struct AnimationUnitKeyHash
        {
            std::size_t operator()(const AnimationUnitKey& key) const noexcept;
        };

        // 此函数为当前核心阶段重建标题、说明和全部选项按钮。
        void rebuildChoices();

        // 此函数把一个选择项转换为当前阶段对应的 A 方命令。
        void submitChoice(const Choice& choice);

        // 此函数在分队选择页把当前悬停分队的完整效果显示在空白区域。
        void refreshChoiceHover();

        // 此函数根据最新快照同步 HUD、页签和准备按钮状态。
        void refreshPreparationWidgets();

        // 此函数仅在死亡单位 ID 列表变化时重建最多八个复活按钮。
        void syncReviveCards();

        // 此函数按单位 ID 查询配置中的中文名称并保留 ID 回退。
        sf::String unitName(const std::string& unitId) const;

        // 此函数返回指定备用槽的固定像素矩形。
        static sf::FloatRect reserveSlotBounds(std::size_t slot) noexcept;

        // 此函数返回接收活动单位出售拖拽的固定像素矩形。
        static sf::FloatRect sellZoneBounds() noexcept;

        // 此函数在鼠标按下位置查找己方可拖拽活动单位。
        core::OwnedUnitId hitTestOwnedUnit(sf::Vector2f pixel) const noexcept;

        // 此函数根据拖拽鼠标位置更新己方空部署格的纯界面预览。
        void updateDeploymentPreview(sf::Vector2f pixel) noexcept;

        // 此函数根据鼠标释放区域生成部署或回备用区命令。
        void finishBasicDrag(sf::Vector2f pixel);

        // 此函数按 ID 查找只读快照中的己方活动单位。
        const core::OwnedUnit* findActiveUnit(
            core::OwnedUnitId unitId) const noexcept;

        // 此函数绘制备用槽、己方/敌方部署单位和拖拽跟随标记。
        void drawPreparationUnits(sf::RenderTarget& target) const;

        // 此函数绘制当前战斗单位、目标连线和实时状态条。
        void drawCombatUnits(sf::RenderTarget& target) const;

        // 此函数根据战斗快照同步 HUD 和选中单位状态。
        void refreshCombatWidgets();

        // 此函数刷新回合结算页的下一回合提示与倒计时。
        void refreshSettlementCountdown();

        // 此函数在连续战斗坐标上命中离鼠标最近的己方单位。
        core::BattleUnitId hitTestBattleUnit(sf::Vector2f pixel) const noexcept;

        // 此函数按单位 ID 获取或创建共享资源对应的独立 Spine 实例。
        UnitAnimationInstance* animationFor(
            core::MapSide side,
            core::OwnedUnitId unitId,
            const core::UnitIdentity& identity);

        // 此函数同时移除一个单位的 Spine 实例及其全部跨帧表现状态。
        void discardAnimation(const AnimationUnitKey& key) noexcept;

        // 此函数将当前准备/战斗快照转换为 relax、move、attack、die。
        void syncAnimations();

        // 此函数绘制带明确红色边框和标题的出售投放区。
        void drawSellZone(sf::RenderTarget& target) const;

        // 此函数显示不经过核心的界面层拖拽提示。
        void showLocalMessage(const sf::String& text, bool success);

        const sf::Font& font_;
        const sf::Font& englishFont_;
        const sf::Font& guardFont_;
        const core::ConfigBundle& config_;
        core::ReadOnlyGameView view_;
        core::MatchPhase builtPhase_ = core::MatchPhase::MatchResult;
        sf::Text title_;
        sf::Text hint_;
        sf::Text hud_;
        sf::Text timerHud_;
        sf::Text hoverInfo_;
        sf::Text reserveTitle_;
        sf::Text message_;
        std::vector<Choice> choices_;
        std::vector<ShopCard> shopCards_;
        std::vector<ReviveCard> reviveCards_;
        BoardTransform boardTransform_;
        std::unique_ptr<Button> purchaseTabButton_;
        std::unique_ptr<Button> reviveTabButton_;
        std::unique_ptr<Button> refreshButton_;
        std::unique_ptr<Button> startButton_;
        bool showReviveList_ = false;
        sf::Clock messageClock_;
        DragState drag_;
        // 此字段只保存拖拽期间当前可预览的己方空部署格。
        std::optional<core::GridPosition> deploymentPreview_;
        UiAction pendingAction_;
        // 此代码块保存暂停状态和后续交互控件的稳定句柄。
        bool paused_ = false;
        std::unique_ptr<Button> pauseButton_;
        std::unique_ptr<Button> resumeButton_;
        std::unique_ptr<Button> restartButton_;
        std::unique_ptr<Button> playAgainButton_;
        std::unique_ptr<Button> resultMenuButton_;
        sf::Text combatHud_;
        sf::Text guardHud_;
        sf::Text settlementHud_;
        sf::Text selectedHud_;
        std::uint64_t settlementFramesRemaining_ = 0;
        core::BattleUnitId selectedBattleUnitId_ = core::InvalidBattleUnitId;
        SpineAssetRepository animationRepository_;
        std::unordered_map<AnimationUnitKey, std::unique_ptr<UnitAnimationInstance>, AnimationUnitKeyHash> animations_;
        std::unordered_map<AnimationUnitKey, std::uint64_t, AnimationUnitKeyHash> lastActionSequences_;
        std::unordered_map<AnimationUnitKey, core::BattlePosition, AnimationUnitKeyHash> lastBattlePositions_;
        // 此表独立记录每个死亡单位的退场完成状态，避免尸体持续绘制。
        std::unordered_map<AnimationUnitKey, bool, AnimationUnitKeyHash> deathAnimationsFinished_;
        bool animationCombatActive_ = false;
    };
}
