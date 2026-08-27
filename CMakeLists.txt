cmake_minimum_required(VERSION 3.14)
project(CET VERSION 0.1.0 LANGUAGES C)

# Generate compile_commands.json for clangd / language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)

file(GLOB_RECURSE CFILES CONFIGURE_DEPENDS src/*.c)

# Add custom Find modules path (for Findraylib.cmake)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Add raylib local installation to search path
list(APPEND CMAKE_PREFIX_PATH "D:/Packages/raylib")

# ─── Source files ─────────────────────────────────────────────────────────────
add_executable(
    CET
    main.c
    ${CFILES}
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

# Windows 专用链接：dwmapi（DwmSetWindowAttribute，用于禁用 DWM 窗口过渡
# 动画，消除全屏/小窗切换时的合成卡顿）
if(WIN32)
    target_link_libraries(CET PRIVATE dwmapi)
endif()

# Copy assets directory to the executable output directory after each build.
# This ensures assets are always found next to the binary regardless of IDE,
# build preset, or deployment scenario.
add_custom_command(TARGET CET POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:CET>/assets"
    COMMENT "Copying assets to build output directory..."
)
