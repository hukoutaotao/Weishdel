#include "game/controllers/HumanController.hpp"

#include <utility>

namespace autochess::game
{
    // 此构造函数固定真人控制器在整局中负责的阵营。
    HumanController::HumanController(const core::MapSide side)
        : side_(side)
    {
    }

    // 此函数返回真人控制器不可变的负责阵营。
    core::MapSide HumanController::side() const noexcept
    {
        return side_;
    }

    // 此函数忽略可读快照并按先进先出顺序取出至多一条鼠标命令。
    std::optional<core::GameCommand> HumanController::nextCommand(
        const core::ReadOnlyGameView&)
    {
        // 此代码块在没有鼠标命令时明确返回空值。
        if (commands_.empty())
        {
            return std::nullopt;
        }

        core::GameCommand command = std::move(commands_.front());
        commands_.pop_front();
        return command;
    }

    // 此函数只接受属于本控制器阵营的命令以阻止界面越权。
    void HumanController::enqueue(core::GameCommand command)
    {
        // 此代码块拒绝未知阵营以及不属于真人一方的界面命令。
        if (side_ == core::MapSide::Unknown || command.actor != side_)
        {
            return;
        }
        commands_.push_back(std::move(command));
    }

    // 此函数释放队列中所有尚未提交的旧对局命令。
    void HumanController::clear() noexcept
    {
        commands_.clear();
    }
}
