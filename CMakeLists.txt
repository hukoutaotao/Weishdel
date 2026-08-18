cmake_minimum_required(VERSION 3.25)

project(
    AutoChess
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT MSVC)
    message(FATAL_ERROR "The current project requires the MSVC compiler.")
endif()

set(SFML_STATIC_LIBRARIES FALSE)

list(
    PREPEND CMAKE_PREFIX_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SFML"
)

find_package(
    SFML 2.6.2
    CONFIG
    REQUIRED
    COMPONENTS graphics window system
)

add_library(
    AutoChessCore
    STATIC
    src/core/Core.cpp
    src/core/Core.hpp
    src/core/config/ConfigError.hpp
    # 将整包配置加载入口编译进纯 C++ 核心静态库。
    src/core/config/ConfigBundleLoader.cpp
    src/core/config/ConfigBundleLoader.hpp
    # 将定义加载器和通用字段读取工具编译进纯 C++ 核心静态库。
    src/core/config/DefinitionConfigLoader.cpp
    src/core/config/DefinitionConfigLoader.hpp
    src/core/config/GameConfigLoader.cpp
    src/core/config/GameConfigLoader.hpp
    src/core/config/MapConfigLoader.cpp
    src/core/config/MapConfigLoader.hpp
    # 将真人、脚本和未来 AI 共用的控制器抽象接口加入核心目标。
    src/core/controllers/IPlayerController.hpp
    # 将第九天策略抽象接口编译进纯 C++ 核心库。
    src/core/ai/IAiStrategy.hpp
    src/core/ai/AiDecisionSupport.cpp
    src/core/ai/AiDecisionSupport.hpp
    src/core/config/ConfigParser.cpp
    src/core/config/ConfigParser.hpp
    src/core/config/ConfigValueReader.cpp
    src/core/config/ConfigValueReader.hpp
    src/core/combat/BattleSetupService.cpp
    src/core/combat/BattleSetupService.hpp
    # 将等级和分队属性解析集中为可独立测试的纯核心服务。
    src/core/combat/BattleStatResolver.cpp
    src/core/combat/BattleStatResolver.hpp
    src/core/combat/BattleSimulation.cpp
    src/core/combat/BattleSimulation.hpp
    src/core/combat/BattleTypes.hpp
    src/core/combat/CombatRules.cpp
    src/core/combat/CombatRules.hpp
    src/core/combat/TargetSelector.cpp
    src/core/combat/TargetSelector.hpp
    src/core/economy/EconomyTypes.hpp
    src/core/economy/DeploymentService.cpp
    src/core/economy/DeploymentService.hpp
    # 将准备阶段的持久单位合成服务编译进核心静态库。
    src/core/economy/MergeService.cpp
    src/core/economy/MergeService.hpp
    src/core/economy/PriceRules.cpp
    src/core/economy/PriceRules.hpp
    # 将出售、死亡转移和复活服务编译进核心静态库。
    src/core/economy/RosterService.cpp
    src/core/economy/RosterService.hpp
    src/core/economy/ShopService.cpp
    src/core/economy/ShopService.hpp
    src/core/map/MapTypes.hpp
    # 将对局阶段、统一命令和日志类型编译进纯 C++ 核心库。
    src/core/match/GameCommand.hpp
    # 将对局选择状态机和统一命令入口编译进核心库。
    src/core/match/Match.cpp
    src/core/match/Match.hpp
    src/core/match/MatchTypes.cpp
    src/core/match/MatchTypes.hpp
    # 将按阵营裁剪的只读游戏快照类型暴露给界面和控制器。
    src/core/match/ReadOnlyGameView.hpp
    # 将回合收入、败者补助和商店重建事务编译进核心库。
    src/core/match/RoundController.cpp
    src/core/match/RoundController.hpp
    src/core/model/Definitions.hpp
    src/core/model/PlayerStateService.cpp
    src/core/model/PlayerStateService.hpp
    src/core/model/PlayerTypes.hpp
    # 将第 5 天单位继承体系和技能抽象接口编译进核心静态库。
    src/core/skills/Skill.cpp
    src/core/skills/Skill.hpp
    # 将技力恢复和确定性技能选目标规则编译进核心静态库。
    src/core/skills/SkillSystem.cpp
    src/core/skills/SkillSystem.hpp
    src/core/units/Unit.cpp
    src/core/units/Unit.hpp
)

target_include_directories(
    AutoChessCore
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(
    AutoChessCore
    PRIVATE
    /W4
    /utf-8
)

add_executable(
    AutoChessGame
    # 将第七天窗口、资源和固定步长运行骨架编译进游戏目标。
    src/game/GameApp.cpp
    src/game/GameApp.hpp
    # 将真人命令队列和第七天占位电脑控制器编译进游戏目标。
    src/game/controllers/HumanController.cpp
    src/game/controllers/HumanController.hpp
    src/game/controllers/BootstrapComputerController.cpp
    src/game/controllers/BootstrapComputerController.hpp
    # 将地图格子换算和色块路线绘制器编译进表现层。
    src/game/rendering/BoardTransform.cpp
    src/game/rendering/BoardTransform.hpp
    src/game/rendering/MapRenderer.cpp
    src/game/rendering/MapRenderer.hpp
    src/game/rendering/UnitRenderer.cpp
    src/game/rendering/UnitRenderer.hpp
    # 将抽象屏幕、主菜单、帮助页和通用按钮编译进表现层。
    src/game/screens/Screen.hpp
    src/game/screens/MainMenuScreen.cpp
    src/game/screens/MainMenuScreen.hpp
    src/game/screens/HelpScreen.cpp
    src/game/screens/HelpScreen.hpp
    src/game/screens/MatchScreen.cpp
    src/game/screens/MatchScreen.hpp
    src/game/ui/Button.cpp
    src/game/ui/Button.hpp
    src/game/ui/UiAction.hpp
    src/game/main.cpp
)

target_link_libraries(
    AutoChessGame
    PRIVATE
    AutoChessCore
    sfml-graphics
    sfml-window
    sfml-system
)

target_compile_options(
    AutoChessGame
    PRIVATE
    /W4
    /utf-8
)

# 向图形程序提供开发期正式数据目录，避免启动目录变化导致配置缺失。
target_compile_definitions(
    AutoChessGame
    PRIVATE
    AUTOCHESS_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

add_custom_command(
    TARGET AutoChessGame
    POST_BUILD

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:sfml-graphics>"
        "$<TARGET_FILE_DIR:AutoChessGame>"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:sfml-window>"
        "$<TARGET_FILE_DIR:AutoChessGame>"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:sfml-system>"
        "$<TARGET_FILE_DIR:AutoChessGame>"

    VERBATIM
)

add_executable(
    AutoChessTests
    tests/TestMain.cpp
    # 将第六天经济、胜负和非法命令验收矩阵编译进测试目标。
    tests/MatchAcceptanceTests.cpp
    tests/MatchAcceptanceTests.hpp
    # 将脚本控制器完整对局测试编译为独立测试源文件。
    tests/MatchScriptedTests.cpp
    tests/MatchScriptedTests.hpp
)

target_link_libraries(
    AutoChessTests
    PRIVATE
    AutoChessCore
)

target_compile_options(
    AutoChessTests
    PRIVATE
    /W4
    /utf-8
)

# 向测试程序提供正式数据与非法测试样本的稳定源目录。
target_compile_definitions(
    AutoChessTests
    PRIVATE
    AUTOCHESS_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    AUTOCHESS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
)
