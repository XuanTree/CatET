cmake_minimum_required(VERSION 3.21)
project(CatET VERSION 1.0.0 LANGUAGES C)

# ─── 版本号 / 组织名（版本号可被 CI 注入）──────────────────────────────────────
# GitHub Actions 打 tag 发版时通过环境变量注入版本号，使安装包文件名、包元数据
# 与 exe 属性页版本都和 tag 保持一致：
# CATET_VERSION=0.7.0 cmake -S . -B out/build/xxx
# 未设置时回退到 project(... VERSION ...)，本地构建行为完全不变。
# 先 STRIP：环境变量常带尾部空白/换行（shell 或 CI 传入），不清掉会污染
# 安装包文件名与版本元数据。
string(STRIP "$ENV{CATET_VERSION}" CATET_VERSION)

if(NOT CATET_VERSION)
    set(CATET_VERSION "${PROJECT_VERSION}")
endif()

# 版本号拆成三段数字：Windows 的 VERSIONINFO 资源只接受逗号分隔的整数，
# 无法直接写 tag 字符串。非数字版本号统一退化为 0，避免 windres 编译失败。
if(CATET_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    set(CATET_VERSION_MAJOR "${CMAKE_MATCH_1}")
    set(CATET_VERSION_MINOR "${CMAKE_MATCH_2}")
    set(CATET_VERSION_PATCH "${CMAKE_MATCH_3}")
elseif(CATET_VERSION MATCHES "^([0-9]+)\\.([0-9]+)")
    set(CATET_VERSION_MAJOR "${CMAKE_MATCH_1}")
    set(CATET_VERSION_MINOR "${CMAKE_MATCH_2}")
    set(CATET_VERSION_PATCH "0")
else()
    set(CATET_VERSION_MAJOR "0")
    set(CATET_VERSION_MINOR "0")
    set(CATET_VERSION_PATCH "0")
endif()

# 组织/公司名：CPack 的 vendor 与 Windows 资源文件的 CompanyName 共用这一处定义
set(CATET_VENDOR "XuanTree")

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
    FetchContent_MakeAvailable(raylib)
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

    # 嵌入程序图标与版本信息（MinGW 工具链自带 windres 编译 .rc 资源）：
    # 由 packaging/CatET.rc.in 生成，版本号取自 CATET_VERSION，避免像以前那样在
    # .rc 里手写版本导致 exe 属性页显示过期版本。
    set(CATET_RC_IN "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CatET.rc.in")

    if(EXISTS "${CATET_RC_IN}")
        # 图标给绝对路径（正斜杠），因为生成物位于构建目录，相对路径会失效
        file(TO_CMAKE_PATH
            "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CatET.ico" CATET_RC_ICON)
        set(CATET_VERSION_COMMA
            "${CATET_VERSION_MAJOR},${CATET_VERSION_MINOR},${CATET_VERSION_PATCH},0")
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
        configure_file("${CATET_RC_IN}"
            "${CMAKE_CURRENT_BINARY_DIR}/generated/CatET.rc" @ONLY)
        target_sources(CatET PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/generated/CatET.rc"
        )
    else()
        message(WARNING "缺少 packaging/CatET.rc.in，本次构建的 exe 将没有图标与版本信息")
    endif()

    # 图形界面子系统（等价于 -mwindows）：
    # 双击运行时不再弹出黑色控制台/终端窗口
    set_target_properties(CatET PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# macOS：可执行文件必须是 .app bundle，CPack 的 Bundle / DragNDrop 生成器才能
# 生成 .app 与 .dmg；否则打包阶段会直接报错退出。
if(APPLE)
    set_target_properties(CatET PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "CatET"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.xuantree.catet"
        MACOSX_BUNDLE_BUNDLE_VERSION "${CATET_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${CATET_VERSION}"
    )

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in")
        set_target_properties(CatET PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST
            "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in"
        )
    endif()
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
# - Linux   → DEB / RPM 安装包
# ═══════════════════════════════════════════════════════════════════════════════
set(CPACK_PACKAGE_NAME "CatET")
set(CPACK_PACKAGE_VENDOR "${CATET_VENDOR}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CatET")

# 版本元数据统一取自 CATET_VERSION（CI 可注入）；三段数字直接复用文件顶部已
# 解析的结果（NSIS / Bundle / DEB / RPM 生成器都要用）。
set(CPACK_PACKAGE_VERSION "${CATET_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${CATET_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${CATET_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${CATET_VERSION_PATCH}")

set(CPACK_PACKAGE_INSTALL_DIRECTORY "CatET")

# ── 平台与架构标识（统一安装包命名）──────────────────────────────────────
# 平台/架构由当前工具链与宿主推导，而非硬编码，避免产物命名不规范。
# 命名统一为  CatET-<版本>-<平台>-<架构>.<扩展名>；
# Windows 产物是引导式安装程序（NSIS 向导），平台后再加 -Setup 以示区分：
# CatET-1.0.0-Windows-Setup-x64.exe
# CatET-1.0.0-Linux-x64.deb  /  CatET-1.0.0-Linux-x64.rpm
# CatET-1.0.0-MacOS-arm64.dmg
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
# 统一为 CatET-<版本>-<平台>-<架构>（Windows 为 -Setup 变体，见下方 WIN32 分支）。
set(CPACK_PACKAGE_FILE_NAME
    "CatET-${CATET_VERSION}-${CATET_PLATFORM}-${CATET_ARCH}")

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

    # 引导式安装程序：文件名在平台后追加 -Setup
    # CatET-1.0.0-Windows-Setup-x64.exe
    set(CPACK_PACKAGE_FILE_NAME
        "CatET-${CATET_VERSION}-${CATET_PLATFORM}-Setup-${CATET_ARCH}")

    # 安装/卸载向导图标（需要 .ico 文件；存在时才启用，避免打包失败）
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CatET.ico")
        set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CatET.ico")
        set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CatET.ico")
    endif()
elseif(APPLE)
    # ── macOS：.app Bundle + DMG 拖拽安装镜像 ──────────────────────────────
    set(CPACK_GENERATOR "Bundle;DragNDrop")
    set(CPACK_BUNDLE_NAME "CatET")

    # 自定义 Info.plist（内含 @CPACK_PACKAGE_VERSION@，由 CPack 在打包时替换）
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in")
        set(CPACK_BUNDLE_PLIST
            "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in")
    endif()

    # 应用/镜像图标（可选，需要 .icns 文件）。
    # macOS 要求 Info.plist 的 CFBundleIconFile 与 Resources 下的图标同名，
    # 这里优先 CatET.icns（与 CPACK_BUNDLE_NAME / CFBundleIconFile 一致），
    # 同时兼容历史命名 CET.icns。
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CatET.icns")
        set(CPACK_BUNDLE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CatET.icns")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CET.icns")
        set(CPACK_BUNDLE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CET.icns")
    endif()
else()
    # ── Linux：DEB / RPM 系统安装包 ────────────────────────────────────────
    # Linux 只产出系统安装包（deb/rpm）；便携 tar.gz 已按需求移除。
    set(CPACK_GENERATOR "DEB;RPM")

    # 安装到 /opt/CatET 下，保持自包含布局（资源已内嵌进可执行文件）
    set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/CatET")
    set(CPACK_DEBIAN_PACKAGE_NAME "catet")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "XuanTree <3304748409@qq.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "games")

    # 包内架构元数据使用规范值（amd64 / arm64），与文件名短名（x64）区分
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CATET_DEB_ARCH}")

    # raylib 为静态链接，不需要 dpkg-shlibdeps 分析动态库依赖
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)

    # RPM 元数据（需要 rpmbuild 才能生成 rpm）
    set(CPACK_RPM_PACKAGE_NAME "catet")
    set(CPACK_RPM_PACKAGE_LICENSE "GPLv3+")
    set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
    set(CPACK_RPM_PACKAGE_VENDOR "XuanTree")

    # 包内架构元数据使用规范值（x86_64 / aarch64），与文件名短名（x64）区分
    set(CPACK_RPM_PACKAGE_ARCHITECTURE "${CATET_RPM_ARCH}")

    # 桌面快捷方式（文件缺失时跳过，避免安装阶段直接失败）
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CatET.desktop")
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CatET.desktop"
            DESTINATION "/usr/share/applications"
        )
    endif()
endif()

include(CPack)
