#pragma once

#include <filesystem>

namespace autochess::game
{
    // 此函数返回当前运行中可执行文件所在的绝对目录。
    std::filesystem::path executableDirectory();

    // 此函数优先选择程序旁的 data 目录，并在其不存在时回退到开发目录。
    std::filesystem::path selectDataDirectory(
        const std::filesystem::path& executableDirectory,
        const std::filesystem::path& developmentDataDirectory);
}
