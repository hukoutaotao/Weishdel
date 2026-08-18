#include "game/GameApp.hpp"

int main()
{
    // 此代码块把程序入口交给统一管理窗口、资源和固定步长的应用对象。
    autochess::game::GameApp application;
    return application.run();
}
