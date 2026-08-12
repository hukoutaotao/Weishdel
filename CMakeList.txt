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