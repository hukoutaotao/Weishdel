#pragma once

#include "core/match/GameCommand.hpp"

#include <optional>

namespace autochess::game
{
    // 此枚举描述屏幕能够请求的应用导航和对局操作。
    enum class UiActionKind
    {
        None,
        StartGame,
        ShowHelp,
        BackToMenu,
        ExitApplication,
        SubmitCommand,
        TogglePause,
        RestartMatch,
        PlayAgain
    };

    // 此结构把一次界面请求和可选核心命令封装为可消费消息。
    struct UiAction
    {
        UiActionKind kind = UiActionKind::None;
        std::optional<core::GameCommand> command;
    };
}
