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
    src/core/config/ConfigParser.cpp
    src/core/config/ConfigParser.hpp
    src/core/config/ConfigValueReader.cpp
    src/core/config/ConfigValueReader.hpp
    src/core/economy/EconomyTypes.hpp
    src/core/economy/DeploymentService.cpp
    src/core/economy/DeploymentService.hpp
    src/core/economy/PriceRules.cpp
    src/core/economy/PriceRules.hpp
    src/core/economy/ShopService.cpp
    src/core/economy/ShopService.hpp
    src/core/map/MapTypes.hpp
    src/core/model/Definitions.hpp
    src/core/model/PlayerStateService.cpp
    src/core/model/PlayerStateService.hpp
    src/core/model/PlayerTypes.hpp
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
