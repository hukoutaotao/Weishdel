#include "game/GameApp.hpp"
#include "game/RuntimePaths.hpp"

#include <filesystem>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

int main()
{
    // 在创建 SFML 窗口前启用每显示器 DPI 感知，避免 Windows 再次缩放整幅画面。
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // 此代码块优先选择程序旁 data，并为开发构建保留源码数据目录回退。
    const std::filesystem::path dataDirectory =
        autochess::game::selectDataDirectory(
            autochess::game::executableDirectory(),
            std::filesystem::path(AUTOCHESS_DATA_DIR));

    // 此代码块把选定资源目录交给统一管理窗口和固定步长的应用对象。
    autochess::game::GameApp application(dataDirectory);
    return application.run();
}
