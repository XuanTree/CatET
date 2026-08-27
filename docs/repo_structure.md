# 仓库目录结构（Repo Structure）

> 本文档描述 CatET 项目的**目录（文件夹）层级规划**，并对照 [`game_instructions.md`](game_instructions.md) 的游戏设计需求，说明每个目录的职责与承载的模块。实际落地以各目录下的头文件/源码为准。

## 1. 总览

```
CET/
├── main.c                 # 程序入口-main函数
├── CMakeLists.txt         # 顶层构建脚本（已使用 file(GLOB_RECURSE src/*.c)）
├── CMakePresets.json      # CMake 预设
├── .gitignore             # 版本忽略规则
├── .clangd                # clangd 语言服务器配置
├── assets/                # 资源目录
│   ├── music/             # 背景音乐
│   ├── sounds/            # 音效
│   ├── sprites/           # 精灵贴图
│   ├── words/             # 词库（CET-4 / CET-6）
│   ├── fonts/             # 字体资源：全英文 UI 所需
│   └── data/              # 运行时持久化数据：save.json（最佳通关时间）
├── cmake/                 # CMake 辅助模块（如 Findraylib.cmake）
├── docs/                  # 项目文档（玩法、架构、目录规划）
├── include/               # 头文件（公共接口）
│   ├── core/              # 框架层：窗口 / 场景栈 / 全局配置
│   ├── entities/          # 实体层：玩家 / 平台 / 关卡对象
│   ├── scenes/            # 场景层：菜单 / 关卡 / 战斗 / 暂停 / 结算 / 设置
│   ├── systems/           # 系统层：关卡流程 / 词库解析 / 持久化 / 速通计时 等
│   └── tools/             # 工具层：镜头 / 计时 / 随机 / 字符串 / 动画 / GUI 等
└── src/                   # 实现架构（与 include 一一对应）
    ├── core/
    ├── entities/
    ├── scenes/
    ├── systems/
    └── tools/
```

## 2. 顶层

| 路径 | 职责 |
|---|---|
| `main.c` | 程序入口，调用 [`Run()`](game_stack_architecture.md) 启动游戏 |
| `CMakeLists.txt` | 顶层构建；`file(GLOB_RECURSE src/*.c)` 使新增源码无需改构建脚本 |
| `CMakePresets.json` | 编译预设（gcc-mingw / MSVC 等） |
| `.gitignore` | 忽略构建产物（`out/`、`.idea/` 等） |
| `.clangd` | 编辑器索引/补全配置 |

## 3. assets/ 资源目录

| 目录 | 内容 | 设计依据 |
|---|---|---|
| `sprites/` | 玩家（idle/jump/run/sleep）、平台、图标；后续扩展 Boss / 敌怪 / 旗 / 字母 / UI | 玩家与各关卡实体 |
| `words/` | `CET4.txt`、`CET6.txt` 词库（格式：`单词<TAB>词性 释义`） | 三难度词源（[`game_instructions.md`](game_instructions.md:7)） |
| `music/` | 背景音乐（当前为空） | — |
| `sounds/` | 音效（当前为空） | — |
| `fonts/` | 英文字体；界面全英文，需显式字体资源 | 全英文 UI（[`game_instructions.md`](game_instructions.md:109)） |
| `data/` | 持久化数据（`save.json`，记录最佳通关时间） | 最佳时间永久存储（[`game_instructions.md`](game_instructions.md:101)） |

music/及sounds/后续可能不会公开上传到仓库中

## 4. include/ 与 src/（头文件 ↔ 实现一一对应）

### 4.1 core/ — 框架层

| 模块 | 职责 |
|---|---|
| `gameapp.h/.c` | 窗口 / 固定分辨率 RenderTexture / 等比缩放绘制 / 音频设备 |
| `gamestack.h/.c` | `GameScene` + `GameStack` 场景栈（含延迟请求队列、生命周期状态机） |
| `game_config.h` | 全局常量（逻辑分辨率、帧率等） |

### 4.2 entities/ — 实体层

现有：`character`、`player`、`platform`。

按设计需求需扩展（文件级，目录不变）：
- `falling_letter`：极速拼写 / Boss 关天上掉落的字母（可捡起放下）
- `enemy`：平台跳跃敌怪（触发战斗场景）
- `boss`：Boss 关敌怪（弹幕 + 自身生命值）
- `bullet`：敌怪 / Boss 弹幕
- `flag`：平台跳跃终点小红旗
- `maze`：迷宫解密 2D 迷宫（Rectangle 组成）

### 4.3 scenes/ — 场景层

架构文档规划的场景均在 `scenes/` 下落地：

| 场景 | 设计依据 |
|---|---|
| `scene_menu` | 开始界面：难度选择、最佳时间展示、Z/X 操作（[`game_instructions.md`](game_instructions.md:101)） |
| `scene_level` | 通用关卡（参数化 `levelId`），承载极速拼写 / 迷宫解密 / 平台跳跃 / Boss 四种关卡 |
| `scene_battle` | 战斗场景：回合制选词 + 弹幕躲避（[`game_instructions.md`](game_instructions.md:61)） |
| `scene_pause` | 暂停覆盖层 |
| `scene_gameover` | 失败 / 胜利结算（HP 归 0 失败；100 关通关胜利） |
| `scene_settings` | 设置界面 |

> 现有 `scene_test` 为骨架验证用测试场景，后续按上述场景拆分。

### 4.4 systems/ — 系统层（新增目录）

承载跨场景的游戏逻辑（区别于 core 的框架职责）：

| 模块 | 职责 | 设计依据 |
|---|---|---|
| `level_flow` | 关卡流程：100 关推进、5:3:2 权重刷新、每 20 关 Boss 关卡、难度词库切换 | 权重与 Boss 节奏（[`game_instructions.md`](game_instructions.md:79)、[`game_instructions.md`](game_instructions.md:95)） |
| `words_loader` | CET4/CET6 词库加载与解析（单词 / 词性 / 释义，多释义取用约定） | 拼写与战斗选词数据源 |
| `save_data` | 最佳通关时间读写（`assets/data/save.json`）——已实现 | 数据持久化（[`game_instructions.md`](game_instructions.md:112)） |
| `speedrun` | 隐式全局计时器：进入第一关开始计时、失败/通关结束、仅成功通关记录最佳时间（配合 `save_data` 持久化与开始菜单显示）——已实现 | 全局计时（[`game_instructions.md`](game_instructions.md:23)） |

### 4.5 tools/ — 工具层

现有：`animation`、`camera`、`genrandom`、`hud`、`raygui`、`strings`、`timer`。提供与具体玩法无关的通用能力，供场景/实体/系统复用。
其中 `hud`（`include/tools/hud.h` / `src/tools/hud.c`）为从 `scene_test` 抽离的全局关卡 HUD：生命值条、关卡号、时间、ESC 提示，供各场景复用（生命值由各场景 onEnter 从 `app->playerHealth` 继承后传入）。

## 5. 设计需求 ↔ 目录映射速查

| 设计需求 | 落地目录 |
|---|---|
| 三难度词库 | `assets/words/` + `systems/words_loader` |
| 玩家 / 平台 | `entities/player`、`entities/platform` |
| 极速拼写 / 迷宫 / 平台跳跃 / Boss 关卡 | `scenes/scene_level` + 对应 `entities/*` |
| 战斗场景（回合制选词 + 弹幕） | `scenes/scene_battle` + `entities/enemy|bullet` |
| 100 关进度与权重 | `systems/level_flow` |
| 最佳时间持久化 | `systems/save_data` + `assets/data/` |
| 全英文 UI | `assets/fonts/` + `tools/raygui` |
| 场景栈框架 | `core/gamestack`、`core/gameapp` |

## 6. 约定

- `include/` 与 `src/` 目录名保持一致，新增源码只需放入对应目录，无需改动构建脚本。
- 空目录以 `.gitkeep` 占位，便于纳入版本管理；实际内容后续按模块填充。
- 迷宫由代码程序化生成（Rectangle），无需 `assets/maps/` 预置地图目录，无需绘制地图精灵，减少游戏体积

## 7. 关于C语言的头文件声明设计

采用双重保险机制，即`#ifndef #define --- #endif + #pragma once`，详细可参考已有代码

这么做是为了避免重复拷贝头文件，减少生成文件的体积
