# CatET 代码规范（Code Style Guide）

> 适用项目：CatET（C / raylib 5.0 / CMake 单机游戏）
> 本文档是团队成员统一遵循的编码约定，结合 [`game_stack_architecture.md`](game_stack_architecture.md) 架构设计、[`repo_structure.md`](repo_structure.md) 目录规划与现有源码风格提炼。
> 代码评审以本文档为准绳。

---

## 1. 命名规则

### 1.1 文件命名
- 一律小写 `snake_case`：`scene_menu.c`、`words_loader.c`、`game_config.h`。
- 头文件与实现文件**同名同目录**（`include/` ↔ `src/` 一一对应）。

### 1.2 类型（struct / enum / typedef）
- 使用 `PascalCase`：`GameScene`、`GameStack`、`Player`、`Platform`、`MenuNav`。
- 结构体名与其 `typedef` 名一致；不透明类型（如 [`GameStack`](include/core/gamestack.h:29)）只前向声明，实现在 `.c` 内。

### 1.3 函数
- 格式：**模块前缀 + PascalCase**。
  - 场景工厂：`XxxSceneCreate(...)`（如 [`TestSceneCreate`](src/scenes/scene_test.c:92)、[`PauseSceneCreate`](src/scenes/scene_pause.c:124)）
  - 生命周期回调：`XxxEnter/Update/Draw/Exit/Pause/Resume`（`static`）
  - 实体操作：`InitPlayer`、`UpdatePlayer`、`DrawPlayer`、`PlayerCollision`、`GroundCollision`
  - 工具：`MenuNavInit`、`MenuNavUpdate`、`InitTimer`、`UpdateTimer`
- 仅本文件内部使用的一律 `static`；跨文件调用才对外暴露（不 `static`）。
- 函数名体现行为：`InitXxx`（初始化+加载资源）、`UpdateXxx`（每帧更新）、`DrawXxx`（每帧绘制）。

### 1.4 变量
- 局部变量与结构体字段用 `camelCase`：`platform_m`、`isOnTheGround`、`facingRight`、`spawnPosition`。
- 布尔量用 `is/has/can` 前缀：`isOnTheGround`、`wantsQuit`、`pauseable`、`isPaused`、`isTimerStart`。
- 指针变量通常用前缀 `p_` 区分或直接以对象命名，团队内统一即可，**推荐**直接对象名（`Player *player`）。

### 1.5 常量与宏
- 编译期常量优先 `#define` 全大写 + 下划线：`GAME_SCALE`、`GRAVITY`、`JUMP_SPEED`、`LOGIC_WIDTH`。
- 模块内只读数据用 `static const` + `k` 前缀：`kStartLabels`、`kPauseActions`（见 [`scene_start.c`](src/scenes/scene_start.c:16)）。
- 枚举值 `UPPER_SNAKE_CASE`：`GAME_SCENE_UPDATE_WHEN_HIDDEN`、`MENU_ACTION_CONFIRM`、`START_ACTION_PLAY`。
- **禁止魔法数裸写**：物理常量、地面高度、分辨率、碰撞阈值必须具名（见 §8 与安全审计 M7）。

---

## 2. 目录结构

```
include/ 与 src/ 一一对应；CMake 使用 file(GLOB_RECURSE src/*.c)，新增源码无需改构建脚本
├── core/      框架层：gameapp / gamestack / game_config
├── entities/  实体层：player / platform / character / enemy / flag / falling_letter / bullet / boss / maze
├── scenes/    场景层：scene_menu / scene_level / scene_battle / scene_pause / scene_gameover / scene_settings
├── systems/   系统层：words_loader / level_flow / save_data / speedrun
└── tools/     工具层：camera / animation / timer / menu / genrandom / raygui
```

- **依赖方向**：`scenes → core → entities → tools`，下层绝不反向依赖上层。
- `systems/` 承载**跨场景游戏逻辑**（关卡流程、词库、持久化），`core/` 只承载**框架职责**，二者不可混用。
- 空目录用 `.gitkeep` 占位；迷宫为程序化生成，无需 `assets/maps/`。

---

## 3. 头文件规范

- 采用**双重保护**（项目约定，见 [`repo_structure.md`](repo_structure.md:131)）：
```c
#ifndef SCENE_XXX_H
#define SCENE_XXX_H

#pragma once
// ... 声明 ...
#endif // SCENE_XXX_H
```
- 头文件只放：类型定义、函数声明、必要的文档注释。**不放实现**（inline 工具如 [`animation.h`](include/tools/animation.h:19) 例外）。
- 头文件自包含：能独立编译，不依赖包含顺序。
- 包含最小化：只包含本模块直接使用的头；优先使用 `前向声明`（`typedef struct Xxx Xxx;`）减少耦合。
- 修改公共接口时**同步更新** `docs/` 架构文档与本文档。

---

## 4. 组件 / 实体设计

- 实体统一 `struct + 操作函数` 模式：
  - `InitXxx(Xxx*)`：初始化并加载资源（须在 `InitWindow` 之后调用）
  - `UpdateXxx(Xxx*, float dt)`：每帧更新
  - `DrawXxx(Xxx*)`：每帧绘制
- 结构体实例使用前必须**零初始化**：`= {0}` 或 `calloc(1, sizeof(...))`，杜绝未初始化读取（见 [`scene_test.c`](src/scenes/scene_test.c:23) 注释）。
- 场景私有数据用 `data` 指针（不透明），由工厂函数 `calloc` 分配、`GameStack` 统一释放。
- 一个 `.c` 文件一个主要模块；内部工具函数 `static` 私有，不对外暴露。
- 实体内部魔法数集中在文件头部 `#define`（如 [`player.c`](src/entities/player.c:9) 的 `GRAVITY/MOVE_SPEED`）。

---

## 5. 状态管理

### 5.1 场景切换（禁止直接改栈）
- **禁止**在场景回调内直接修改场景栈；一律调用延迟请求 API（`GameStackPush/Pop/Replace/ClearTo/RequestQuit`），由 [`GameStackUpdate`](src/core/gamestack.c:232) 帧首统一 flush，避免 use-after-free。
- 场景通过 `self->owner` 获取所属栈：`GameStackReplace(self->owner, XxxSceneCreate(...))`。
- `Replace` 用于**永久跳转**（菜单→关卡）；`Push/Pop` 用于**临时覆盖**（暂停层）；`ClearTo` 用于**清空回根**（返回主菜单）。

### 5.2 菜单动作模式
- 菜单/按钮交互采用 **action 枚举**：`Draw` 阶段由按钮/悬停写入 `d->action`，`Update` 阶段统一消费执行，执行后复位为 `NONE`（参考 [`scene_start.c`](src/scenes/scene_start.c:48)）。
- 键盘导航统一用 [`MenuNav`](include/tools/menu.h:22) + raygui `STATE_FOCUSED` 高亮；选中项切换播放 `uiSound`。

### 5.3 暂停状态机
- `app.isPaused` 由主循环统一管理 ESC 进入/退出（[`game.c`](src/game.c:26)）；暂停场景自身**不检测 ESC**，避免同一事件同帧弹出。
- `pauseable` 标记：关卡/玩法场景 `true`，菜单/过场/暂停 `false`。

---

## 6. 样式方案（UI）

- 所有 UI 以**固定逻辑分辨率 640×480** 坐标系绘制（包在 `GameAppBegin/End` 之间），不使用窗口像素坐标。
- 界面**全英文**，无中文备选（[`game_instructions.md`](game_instructions.md:113)）。
- 按钮统一模式：居中垂直排列 + `GuiButton` + 键盘选中 `STATE_FOCUSED` + 鼠标悬停同步 `nav.selected`（参考 [`scene_start.c`](src/scenes/scene_start.c:92)）。
- 覆盖层半透明遮罩用 `Fade(BLACK, alpha)`（参考 [`scene_pause.c`](src/scenes/scene_pause.c:82)）。
- 颜色/字体用 raylib 内置或 `assets/fonts/`；不直接依赖系统字体。

---

## 7. 注释规范

- 使用**中文**注释。
- 注释说明**为什么**，而非是什么；关键权衡与坑点必须写（如 [`gameapp.c`](src/core/gameapp.c:44) 的鼠标坐标变换说明）。
- 文件/模块用等号分隔线分组：`// ─────────────────── 模块名 ───────────────────`。
- 函数上方用行注释说明职责、入参与调用约定。
- 魔法数/硬编码须注释含义与来源；危险操作（资源释放、指针转移）标注 `// 注意：...`。

---

## 8. 错误处理与健壮性

- **资源加载必须判空并降级**：
  - `LoadTexture` → 检查 `texture.id != 0`
  - `LoadImage` → 检查 `image.data != NULL`（参考 [`platform.c`](src/entities/platform.c:33)）
  - 失败时置零尺寸/跳过绘制，不静默继续，可打印 `TraceLog`。
- **动态内存**：`realloc` 返回值必须检查，失败保留旧指针并回滚容量（安全审计 M1）；`malloc/calloc` 与 `free` 严格配对。
- **除零防护**：除法前校验除数（如 `frameCount > 0`，审计 M3）；`range <= 0` 时返回安全值。
- **公共 API 判空**：入口函数对指针参数判空（`if (!x) return;`）。
- **链接一致性**：声明与实现必须同名同签名；新增/改名 API 及时同步头文件（审计 M2）。

---

## 9. 内存与资源管理

- **谁加载谁卸载**：场景资源在 `onEnter` 加载、`onExit` 卸载；栈覆盖/恢复不重复加载。
- 框架级资源（窗口图标、`RenderTexture`、音频设备）由 [`GameAppClose`](src/core/gameapp.c:110) 统一释放。
- `data` 由 `GameStack` 释放；场景持有时不要额外 `free(data)`。
- 字符串处理优先使用 [`strings.c`](src/tools/strings.c:1) 的 `String`（拥有内存、`'\0'` 结尾、可自动扩容）；`StringCreate*` 创建的实例必须 `StringFree` 释放；临时格式化输出仍用 raylib `TextFormat` 缓冲区。
- 禁止使用 `strcpy/strcat/sprintf` 等危险函数（审计确认全项目零使用，保持）。

---

## 10. 提交规范（Git）

采用 Conventional Commits：`type(scope): 描述`，描述用中文。

| type | 用途 |
|---|---|
| `feat` | 新功能 / 新场景 |
| `fix` | 缺陷修复 |
| `refactor` | 重构，不改行为 |
| `docs` | 文档变更 |
| `chore` | 构建 / 工程配置 |
| `perf` | 性能优化 |

示例：
```
feat(scenes): 新增 LevelScene 关卡框架，支持 levelId 参数化
fix(core): gamestack 扩容失败时回滚容量，避免空指针崩溃
docs(code_style): 补充菜单动作模式约定
```
提交粒度：一次提交只做一件事；资源/命名/空行等纯格式化与功能改动分离。

---

## 11. 其他工程实践

- 编译期建议开启 `-Wall -Wextra -Werror`（审计 L12），修复所有警告再提交。
- 随机数：仅进程启动播种一次；避免 `srand(time(NULL))` 每调重播与模偏差（审计 M5）。新增随机需求统一走 `genrandom` 模块化接口。
- 新文件放入对应目录即可，**无需改 CMake**（`GLOB_RECURSE` 自动收集）。
- 地图等程序化内容不新增 `assets/maps/` 目录。
