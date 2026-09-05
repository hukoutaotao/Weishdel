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
    message(FATAL_ERROR "The standalone project requires the MSVC compiler.")
endif()

# Keep the requested standalone layout: the executable is directly under the
# selected build directory, not under an additional Debug/Release subfolder.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# CMake/Ninja must recognize the localized /showIncludes prefix used by MSVC.
if(CMAKE_GENERATOR MATCHES "^Ninja")
    set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "注意: 包含文件:  ")
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${CMAKE_CXX_CL_SHOWINCLUDES_PREFIX}")
endif()

# The source copy contains only the SFML components used by this game. These
# imported targets deliberately avoid a dependency on the original project's
# SFMLConfig.cmake or any path outside this directory.
set(SFML_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SFML")
set(SFML_INCLUDE_DIR "${SFML_ROOT}/include")

foreach(SFML_COMPONENT IN ITEMS system window graphics)
    add_library(sfml-${SFML_COMPONENT} SHARED IMPORTED GLOBAL)
    set_target_properties(sfml-${SFML_COMPONENT} PROPERTIES
        IMPORTED_CONFIGURATIONS RELEASE
        IMPORTED_IMPLIB_RELEASE "${SFML_ROOT}/lib/sfml-${SFML_COMPONENT}.lib"
        IMPORTED_LOCATION_RELEASE "${SFML_ROOT}/bin/sfml-${SFML_COMPONENT}-2.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${SFML_INCLUDE_DIR}"
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        MAP_IMPORTED_CONFIG_MINSIZEREL Release
        MAP_IMPORTED_CONFIG_DEBUG Release
    )
endforeach()

foreach(REQUIRED_FILE IN ITEMS
    "${SFML_ROOT}/include/SFML/Graphics/RenderWindow.hpp"
    "${SFML_ROOT}/lib/sfml-graphics.lib"
    "${SFML_ROOT}/lib/sfml-window.lib"
    "${SFML_ROOT}/lib/sfml-system.lib"
    "${SFML_ROOT}/bin/sfml-graphics-2.dll"
    "${SFML_ROOT}/bin/sfml-window-2.dll"
    "${SFML_ROOT}/bin/sfml-system-2.dll"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/msvc-runtime/msvcp140.dll"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/msvc-runtime/vcruntime140.dll"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/msvc-runtime/vcruntime140_1.dll"
    )
    if(NOT EXISTS "${REQUIRED_FILE}")
        message(FATAL_ERROR "Standalone dependency is missing: ${REQUIRED_FILE}")
    endif()
endforeach()

set(AUTOCHESS_CORE_SOURCES
    src/core/Core.cpp
    src/core/Core.hpp
    src/core/config/ConfigError.hpp
    src/core/config/ConfigBundleLoader.cpp
    src/core/config/ConfigBundleLoader.hpp
    src/core/config/DefinitionConfigLoader.cpp
    src/core/config/DefinitionConfigLoader.hpp
    src/core/config/GameConfigLoader.cpp
    src/core/config/GameConfigLoader.hpp
    src/core/config/MapConfigLoader.cpp
    src/core/config/MapConfigLoader.hpp
    src/core/controllers/IPlayerController.hpp
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
    src/core/economy/MergeService.cpp
    src/core/economy/MergeService.hpp
    src/core/economy/PriceRules.cpp
    src/core/economy/PriceRules.hpp
    src/core/economy/RosterService.cpp
    src/core/economy/RosterService.hpp
    src/core/economy/ShopService.cpp
    src/core/economy/ShopService.hpp
    src/core/map/MapTypes.hpp
    src/core/match/GameCommand.hpp
    src/core/match/Match.cpp
    src/core/match/Match.hpp
    src/core/match/MatchTypes.cpp
    src/core/match/MatchTypes.hpp
    src/core/match/ReadOnlyGameView.hpp
    src/core/match/RoundController.cpp
    src/core/match/RoundController.hpp
    src/core/model/Definitions.hpp
    src/core/model/PlayerStateService.cpp
    src/core/model/PlayerStateService.hpp
    src/core/model/PlayerTypes.hpp
    src/core/units/Unit.cpp
    src/core/units/Unit.hpp
)

add_library(AutoChessCore STATIC ${AUTOCHESS_CORE_SOURCES})
target_include_directories(AutoChessCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_options(AutoChessCore PRIVATE /W4 /utf-8)

# Build the complete Spine 3.8 C++ runtime included in this copy. The SFML
# adapter is part of the same static library so no external Spine package is
# needed at configure or run time.
set(SPINE_RUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spine-runtimes-3.8")
file(GLOB SPINE_CPP_SOURCES CONFIGURE_DEPENDS
    "${SPINE_RUNTIME_ROOT}/spine-cpp/spine-cpp/src/spine/*.cpp"
)
if(NOT SPINE_CPP_SOURCES)
    message(FATAL_ERROR "Spine C++ runtime sources are missing from the standalone copy.")
endif()

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

set(AUTOCHESS_GAME_SOURCES
    src/game/GameApp.cpp
    src/game/GameApp.hpp
    src/game/animation/UnitAnimationManifest.cpp
    src/game/animation/UnitAnimationManifest.hpp
    src/game/animation/SpineAssetRepository.cpp
    src/game/animation/SpineAssetRepository.hpp
    src/game/animation/UnitAnimationInstance.cpp
    src/game/animation/UnitAnimationInstance.hpp
    src/game/RuntimePaths.cpp
    src/game/RuntimePaths.hpp
    src/game/controllers/HumanController.cpp
    src/game/controllers/HumanController.hpp
    src/game/rendering/BoardTransform.cpp
    src/game/rendering/BoardTransform.hpp
    src/game/rendering/MapRenderer.cpp
    src/game/rendering/MapRenderer.hpp
    src/game/rendering/UnitRenderer.cpp
    src/game/rendering/UnitRenderer.hpp
    src/game/screens/Screen.hpp
    src/game/screens/MainMenuScreen.cpp
    src/game/screens/MainMenuScreen.hpp
    src/game/screens/HelpScreen.cpp
    src/game/screens/HelpScreen.hpp
    src/game/screens/MatchScreen.cpp
    src/game/screens/MatchScreen.hpp
    src/game/ui/Button.cpp
    src/game/ui/Button.hpp
    src/game/ui/UiTheme.hpp
    src/game/ui/UiAction.hpp
    src/game/main.cpp
)

add_executable(AutoChessGame ${AUTOCHESS_GAME_SOURCES})
target_include_directories(AutoChessGame PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(
    AutoChessGame
    PRIVATE
    AutoChessCore
    AutoChessSpine
    sfml-graphics
    sfml-window
    sfml-system
)
target_compile_options(AutoChessGame PRIVATE /W4 /utf-8)
target_compile_definitions(
    AutoChessGame
    PRIVATE
    AUTOCHESS_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

# Keep all runtime files next to the executable and also copy data into the
# build output. RuntimePaths still prefers the adjacent data directory, so the
# resulting executable can be moved together with this directory.
add_custom_command(
    TARGET AutoChessGame
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_ROOT}/bin/sfml-graphics-2.dll"
        "$<TARGET_FILE_DIR:AutoChessGame>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_ROOT}/bin/sfml-window-2.dll"
        "$<TARGET_FILE_DIR:AutoChessGame>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_ROOT}/bin/sfml-system-2.dll"
        "$<TARGET_FILE_DIR:AutoChessGame>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/msvc-runtime/msvcp140.dll"
        "$<TARGET_FILE_DIR:AutoChessGame>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/msvc-runtime/vcruntime140.dll"
        "$<TARGET_FILE_DIR:AutoChessGame>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/msvc-runtime/vcruntime140_1.dll"
        "$<TARGET_FILE_DIR:AutoChessGame>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:AutoChessGame>/data"
    VERBATIM
)
