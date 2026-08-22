#include "game/RuntimePaths.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <string>
#include <system_error>
#include <vector>

namespace autochess::game
{
    namespace
    {
        constexpr DWORD InitialExecutablePathCapacity = 260;
    }

    // 此函数通过 Windows 模块路径取得不受当前工作目录影响的程序目录。
    std::filesystem::path executableDirectory()
    {
        std::vector<wchar_t> buffer(InitialExecutablePathCapacity);

        // 此代码块逐步扩大缓冲区，直到完整容纳可执行文件路径。
        while (true)
        {
            const DWORD length = GetModuleFileNameW(
                nullptr,
                buffer.data(),
                static_cast<DWORD>(buffer.size()));
            // 此分支在系统路径查询失败时退回稳定的当前目录。
            if (length == 0)
            {
                std::error_code error;
                const std::filesystem::path currentDirectory =
                    std::filesystem::current_path(error);
                return error
                    ? std::filesystem::path(".")
                    : currentDirectory;
            }
            // 此分支在路径完整时返回其父目录作为程序目录。
            if (length < buffer.size())
            {
                return std::filesystem::path(
                    std::wstring(buffer.data(), length)).parent_path();
            }
            buffer.resize(buffer.size() * 2);
        }
    }

    // 此函数让发布包使用自身 data，同时保留构建目录缺 data 时的开发回退。
    std::filesystem::path selectDataDirectory(
        const std::filesystem::path& executableDirectoryPath,
        const std::filesystem::path& developmentDataDirectory)
    {
        const std::filesystem::path adjacentDataDirectory =
            executableDirectoryPath / "data";
        std::error_code error;

        // 此代码块只要确认程序旁存在目录，就固定使用该目录并暴露其中缺文件错误。
        if (std::filesystem::is_directory(adjacentDataDirectory, error))
        {
            return adjacentDataDirectory;
        }
        return developmentDataDirectory;
    }
}
