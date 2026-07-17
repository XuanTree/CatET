cmake_minimum_required(VERSION 3.14)
project(CET VERSION 0.1.0 LANGUAGES C)

# Generate compile_commands.json for clangd / language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add custom Find modules path (for Findraylib.cmake)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Add raylib local installation to search path (MinGW build at D:/RayLib_MinGW)
list(APPEND CMAKE_PREFIX_PATH "D:/RayLib_MinGW")

# ─── Source files ─────────────────────────────────────────────────────────────
set(CORE_SOURCES
    src/core/scene.c
    src/core/timer.c
    src/core/resource.c
)

set(MATH_SOURCES
    src/math/vector.c
)

set(INPUT_SOURCES
    src/input/input.c
)

set(GRAPHICS_SOURCES
    src/graphics/animation.c
    src/graphics/sprite.c
)

set(ENTITY_SOURCES
    src/entity/player.c
)

add_executable(CET
    main.c
    src/game.c
    include/game.h
)

# Try to find raylib from system first (using custom Findraylib.cmake module)
find_package(raylib QUIET)

if(NOT raylib_FOUND)
    # Automatically download and build raylib if not installed on the system
    include(FetchContent)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.0
    )
    FetchContent_MakeAvailable(raylib)
endif()

target_include_directories(CET PRIVATE include)
target_link_libraries(CET PRIVATE raylib::raylib)
