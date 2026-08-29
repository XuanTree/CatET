cmake_minimum_required(VERSION 3.21)
project(CatET VERSION 0.2.0 LANGUAGES C)

# Generate compile_commands.json for clangd / language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)

file(GLOB_RECURSE CFILES CONFIGURE_DEPENDS src/*.c)

# Add custom Find modules path (for Findraylib.cmake)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Add raylib local installation to search path
list(APPEND CMAKE_PREFIX_PATH "D:/Packages/raylib")

# 内嵌资源生成器依赖 Python（src/tools/pack_assets.py）
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ─── Source files ─────────────────────────────────────────────────────────────
add_executable(
    CatET
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
        GIT_TAG 6.0
    )

    # 仅编译 raylib，跳过其自带 install 规则，避免开发文件（头文件/静态库）
    # 混入安装包（DEB/RPM/TGZ）。
    set(CMAKE_SKIP_INSTALL_RULES TRUE)
    FetchContent_MakeAvailable(raylib)
    set(CMAKE_SKIP_INSTALL_RULES FALSE)
endif()

# 兼容：Windows 的 Findraylib 提供 raylib::raylib；FetchContent 提供的目标是
# raylib，这里补一个别名，保证下方统一用 raylib::raylib 链接。
if(NOT TARGET raylib::raylib AND TARGET raylib)
    add_library(raylib::raylib ALIAS raylib)
endif()

target_include_directories(CatET PRIVATE include)
target_link_libraries(CatET PRIVATE raylib::raylib)

# ─── 内嵌资源生成 ─────────────────────────────────────────────────────────────
# 由 src/tools/pack_assets.py 把 assets/ 编译为 C 字节数组并链接进可执行文件，
# 运行时不依赖外置 assets/ 目录（安装包不暴露资源文件，见 src/tools/resource.c）。
set(EMBEDDED_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(GLOB_RECURSE ASSET_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
add_custom_command(
    OUTPUT "${EMBEDDED_GEN_DIR}/embedded_assets.c"
    "${EMBEDDED_GEN_DIR}/embedded_assets.h"
    COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/pack_assets.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "${EMBEDDED_GEN_DIR}"
    DEPENDS ${ASSET_FILES}
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/pack_assets.py"
    COMMENT "Packing assets into embedded C resources..."
    VERBATIM
)
target_sources(CatET PRIVATE "${EMBEDDED_GEN_DIR}/embedded_assets.c")
target_include_directories(CatET PRIVATE "${EMBEDDED_GEN_DIR}")

# Windows 专用链接：dwmapi（DwmSetWindowAttribute，用于禁用 DWM 窗口过渡
# 动画，消除全屏/小窗切换时的合成卡顿）
if(WIN32)
    target_link_libraries(CatET PRIVATE dwmapi)

    # 嵌入程序图标（MinGW 工具链自带 windres 编译 .rc 资源）
    target_sources(CatET PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CatET.rc"
    )

    # 图形界面子系统（等价于 -mwindows）：
    # 双击运行时不再弹出黑色控制台/终端窗口
    set_target_properties(CatET PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# 开发便利：构建后把 assets 复制到可执行目录，便于直接在 build 目录调试。
# 注意：安装包（CPack）通过下方 install() 规则打包，不含 assets/ ——
# 运行所需的全部资源已由 pack_assets.py 内嵌进可执行文件（内存加载）。
add_custom_command(TARGET CatET POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:CatET>/assets"
    COMMENT "Copying assets to build output directory (dev only)..."
)

# ═══════════════════════════════════════════════════════════════════════════════
# 安装规则（供 CPack 打包使用）
#
# 全部资源（贴图/音效/词库/字体）已由 src/tools/pack_assets.py 编译进可执行
# 文件（见上方"内嵌资源生成"），运行时不读取外置文件，因此安装包不再部署
# assets/ 目录，资源不对外暴露。唯一写文件的是存档 save.json，由
# src/systems/save_data.c 写到可执行文件同级。
# ═══════════════════════════════════════════════════════════════════════════════
install(TARGETS CatET
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)

# 清理 raylib（FetchContent 子项目）自带 install 规则误装入包的头文件/静态库
# 等开发文件，保证安装包只包含运行所需内容（install(CODE) 在安装阶段执行）。
install(CODE "
    file(REMOVE_RECURSE \"\${CMAKE_INSTALL_PREFIX}/include\")
    file(REMOVE_RECURSE \"\${CMAKE_INSTALL_PREFIX}/lib\")
")

# 让 MinGW 构建的可执行文件静态链接 GCC/winpthread 运行库，
# 使 Windows 下的 CatET.exe 不依赖 libgcc_s_seh-1.dll、libwinpthread-1.dll 等
# DLL 即可独立运行（UCRT64 下不支持 -static-libwinpthread，故用 -static）。
if(MINGW)
    target_link_options(CatET PRIVATE
        -static
        -static-libgcc
    )
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# CPack 打包配置
#
# 目标：生成“可独立运行的安装程序”，而不是内含 exe+assets 的普通压缩包：
# - Windows → NSIS 安装向导（.exe setup，含开始菜单快捷方式与卸载程序）
# - macOS   → .app Bundle + DMG 拖拽安装镜像
# - Linux   → DEB / RPM / TGZ 安装包
# ═══════════════════════════════════════════════════════════════════════════════
set(CPACK_PACKAGE_NAME "CatET")
set(CPACK_PACKAGE_VENDOR "XuanTree")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CatET")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "CatET")

# ── 平台与架构标识（统一安装包命名）──────────────────────────────────────
# 平台/架构由当前工具链与宿主推导，而非硬编码，避免产物命名不规范：
# 文件名统一为  CatET-<平台>-<架构>-<版本>
# 例：Windows → CatET-windows-x64-0.2.0.exe
# Linux   → CatET-linux-x64-0.2.0.deb / .rpm / .tar.gz
if(WIN32)
    set(CATET_PLATFORM "Windows")
elseif(APPLE)
    set(CATET_PLATFORM "MacOS")
else()
    set(CATET_PLATFORM "Linux")
endif()

# CMAKE_SYSTEM_PROCESSOR 在 MinGW(UCRT64) 下通常为 AMD64、Linux 下为
# x86_64/aarch64，这里归一化为短名用于文件名。
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    set(CATET_ARCH "x64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(CATET_ARCH "arm64")
else()
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" CATET_ARCH)
endif()

# DEB/RPM 安装包内的架构元数据必须使用发行版规范值（amd64 / x86_64 /
# arm64 / aarch64），与文件名的短名（x64）分开，确保包管理器能正确识别
# 可安装目标架构。
if(CATET_ARCH STREQUAL "x64")
    set(CATET_DEB_ARCH "amd64")
    set(CATET_RPM_ARCH "x86_64")
elseif(CATET_ARCH STREQUAL "arm64")
    set(CATET_DEB_ARCH "arm64")
    set(CATET_RPM_ARCH "aarch64")
else()
    set(CATET_DEB_ARCH "${CATET_ARCH}")
    set(CATET_RPM_ARCH "${CATET_ARCH}")
endif()

# 覆盖各生成器默认命名（win64 / Linux / Debian / RPM 各自规则），
# 统一为 CatET-<平台>-<架构>-<版本>。
set(CPACK_PACKAGE_FILE_NAME
    "CatET-${CATET_PLATFORM}-${CATET_ARCH}-v${PROJECT_VERSION}")

# 安装程序/安装包中展示的许可协议文件
# （NSIS 向导的"许可证"页、DEB/RPM 的 license 元数据均取自这里）
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

if(WIN32)
    # ── Windows：NSIS 标准安装向导 ──────────────────────────────────────────
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_PACKAGE_NAME "CatET")
    set(CPACK_NSIS_DISPLAY_NAME "CatET")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_MENU_LINKS "CatET.exe" "CatET")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "CatET.exe")
    set(CPACK_NSIS_MODIFY_PATH OFF)

    # 安装/卸载向导图标（需要 .ico 文件；存在时才启用，避免打包失败）
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CatET.ico")
        set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CatET.ico")
        set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CatET.ico")
    endif()
elseif(APPLE)
    # ── macOS：.app Bundle + DMG 拖拽安装镜像 ──────────────────────────────
    set(CPACK_GENERATOR "Bundle;DragNDrop")
    set(CPACK_BUNDLE_NAME "CatET")
    set(CPACK_BUNDLE_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in")

    # 应用/镜像图标（可选，需要 .icns 文件）
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CET.icns")
        set(CPACK_BUNDLE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CET.icns")
    endif()
else()
    # ── Linux：DEB / RPM / TGZ 安装包 ──────────────────────────────────────
    set(CPACK_GENERATOR "DEB;RPM;TGZ")

    # 安装到 /opt/CatET 下，保持自包含布局（资源已内嵌进可执行文件）
    set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/CatET")
    set(CPACK_DEBIAN_PACKAGE_NAME "catet")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "XuanTree <xuantree@example.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "games")

    # 包内架构元数据使用规范值（amd64 / arm64），与文件名短名（x64）区分
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CATET_DEB_ARCH}")

    # raylib 为静态链接，不需要 dpkg-shlibdeps 分析动态库依赖
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)

    # RPM 元数据（需要 rpmbuild 才能生成 rpm）
    set(CPACK_RPM_PACKAGE_NAME "catet")
    set(CPACK_RPM_PACKAGE_LICENSE "Proprietary")
    set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
    set(CPACK_RPM_PACKAGE_VENDOR "XuanTree")

    # 包内架构元数据使用规范值（x86_64 / aarch64），与文件名短名（x64）区分
    set(CPACK_RPM_PACKAGE_ARCHITECTURE "${CATET_RPM_ARCH}")

    # 桌面快捷方式
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CatET.desktop"
        DESTINATION "/usr/share/applications"
    )
endif()

include(CPack)
