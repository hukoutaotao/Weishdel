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

# CMake 3.29 may decode localized MSVC /showIncludes output with the wrong
# console code page. Override the prefix used by Ninja so header changes
# reliably invalidate every translation unit that includes them.
if(CMAKE_GENERATOR MATCHES "^Ninja")
    set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "注意: 包含文件:  ")
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${CMAKE_CXX_CL_SHOWINCLUDES_PREFIX}")
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
    src/core/ai/AiController.cpp
    src/core/ai/AiController.hpp
    src/core/ai/AiDecisionSupport.cpp
    src/core/ai/AiDecisionSupport.hpp
    src/core/ai/OffensiveStrategy.cpp
    src/core/ai/OffensiveStrategy.hpp
    src/core/ai/DefensiveStrategy.cpp
    src/core/ai/DefensiveStrategy.hpp
    src/core/ai/RouteStrategy.cpp
    src/core/ai/RouteStrategy.hpp
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

# Spine 3.8 runtime is built as a separate static library so the game target
# only consumes the stable C++/SFML adapter surface.
set(SPINE_RUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spine-runtimes-3.8")
file(GLOB SPINE_CPP_SOURCES CONFIGURE_DEPENDS
    "${SPINE_RUNTIME_ROOT}/spine-cpp/spine-cpp/src/spine/*.cpp"
)

add_library(
    AutoChessSpine
    STATIC
    ${SPINE_CPP_SOURCES}
    "${SPINE_RUNTIME_ROOT}/spine-sfml/cpp/src/spine/spine-sfml.cpp"
)

target_include_directories(
    AutoChessSpine
    PUBLIC
    "${SPINE_RUNTIME_ROOT}/spine-cpp/spine-cpp/include"
    "${SPINE_RUNTIME_ROOT}/spine-sfml/cpp/src"
)

target_link_libraries(
    AutoChessSpine
    PUBLIC
    sfml-graphics
    sfml-window
    sfml-system
)

target_compile_options(
    AutoChessSpine
    PRIVATE
    /W3
    /wd4100
    /wd4127
    /wd4244
    /wd4267
    /wd4305
    /wd4458
    /wd4706
    /utf-8
)
add_executable(
    AutoChessGame
    # 将第七天窗口、资源和固定步长运行骨架编译进游戏目标。
    src/game/GameApp.cpp
    src/game/GameApp.hpp
    # 将可执行文件目录和发布数据路径选择工具编译进游戏目标。
    src/game/RuntimePaths.cpp
    src/game/RuntimePaths.hpp
    # 将真人命令队列编译进表现层游戏目标。
    src/game/controllers/HumanController.cpp
    src/game/controllers/HumanController.hpp
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
    AutoChessSpine
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

    # 此命令把正式配置和地图复制到程序旁，确保构建产物也验证发布路径。
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:AutoChessGame>/data"

    VERBATIM
)

# 此目标从已验证的 Release 产物组装可直接启动的独立提交目录。
set(AUTOCHESS_DIST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dist")
add_custom_target(
    AutoChessDist
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${AUTOCHESS_DIST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${AUTOCHESS_DIST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:AutoChessGame>"
        "${AUTOCHESS_DIST_DIR}/AutoChessGame.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:sfml-graphics>"
        "${AUTOCHESS_DIST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:sfml-window>"
        "${AUTOCHESS_DIST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:sfml-system>"
        "${AUTOCHESS_DIST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/data"
        "${AUTOCHESS_DIST_DIR}/data"
    DEPENDS AutoChessGame
    COMMENT "Assembling standalone AutoChess dist directory"
    VERBATIM
)

add_executable(
    AutoChessTests
    tests/TestMain.cpp
    # 将发布资源路径选择测试和无 SFML 路径工具编译进测试目标。
    tests/RuntimePathsTests.cpp
    tests/RuntimePathsTests.hpp
    src/game/RuntimePaths.cpp
    src/game/RuntimePaths.hpp
    # 将第九天 AI 控制器与三种策略差异测试编译进测试目标。
    tests/AiTests.cpp
    tests/AiTests.hpp
    # 将三策略、双地图和三分队的十八组真实对局矩阵编译进测试目标。
    tests/AiIntegrationTests.cpp
    tests/AiIntegrationTests.hpp
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
