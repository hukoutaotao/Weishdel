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
)