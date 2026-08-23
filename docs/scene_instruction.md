# 场景创建指导（Scene Creation Guide）

> 核心功能：如何在 CatET 中**创建一个新场景**。
> 本文档结合 [`game_stack_architecture.md`](game_stack_architecture.md) 的场景栈架构、[`repo_structure.md`](repo_structure.md) 目录规划、[`code_style.md`](code_style.md) 代码规范与现有源码（[`scene_start.c`](src/scenes/scene_start.c)、[`scene_test.c`](src/scenes/scene_test.c)、[`scene_pause.c`](src/scenes/scene_pause.c)、[`gamestack.c`](src/core/gamestack.c)）编写，保证所有指导均可直接落地执行，并可作为后续开发与代码评审的依据。

---

## 目录

1. [场景的定义与数据模型](#1-场景的定义与数据模型)
2. [创建场景的完整流程](#2-创建场景的完整流程)
3. [关键接口 / 函数设计](#3-关键接口--函数设计)
4. [具体实现步骤与示例代码](#4-具体实现步骤与示例代码)
5. [涉及的文件路径](#5-涉及的文件路径)
6. [与现有模块的集成方式](#6-与现有模块的集成方式)
7. [异常处理与边界情况](#7-异常处理与边界情况)
8. [性能与安全考量](#8-性能与安全考量)
9. [常见问题与排查建议](#9-常见问题与排查建议)
10. [创建场景检查清单](#10-创建场景检查清单)

---

## 1. 场景的定义与数据模型

### 1.1 什么是场景

**场景（Scene）= 一个可独立运行的最小游戏单元**。开始菜单、关卡、暂停、结算、设置、过场都算场景。

- 场景栈 [`GameStack`](include/core/gamestack.h:29) 以**栈**管理场景，**栈顶为唯一活跃场景**（执行 `onUpdate/onDraw`）。
- 压入新场景会**覆盖**当前栈顶（触发其 `onPause`）；弹出后自动**恢复**下层（触发其 `onResume`）。
- 被覆盖的场景通常不销毁、不卸载资源，因此覆盖/恢复开销极低。

```
┌──────────────────────────────┐
│       PauseScene             │  ← 活跃：更新 + 绘制
├──────────────────────────────┤
│       LevelScene             │  ← 被覆盖：暂停，仅按需绘制（半透明底）
├──────────────────────────────┤
│       MenuScene              │  ← 栈底
└──────────────────────────────┘
```

**场景分两类：**
| 类型 | 说明 | 示例 |
|---|---|---|
| 全屏场景 | 独占整个画面与逻辑 | 菜单、关卡、结算 |
| 覆盖层场景 | 盖在其它场景之上，配合半透明遮罩 | 暂停 |

### 1.2 `GameScene` 数据模型

```c
// include/core/gamestack.h（现有实现）
struct GameScene {
  const char *name;       // 场景名（调试用）
  SceneEventFn onEnter;   // 进入：加载本场景资源
  SceneEventFn onExit;    // 永久离开：卸载本场景资源
  SceneUpdateFn onUpdate; // 每帧更新（仅当活跃或被允许隐藏更新）
  SceneDrawFn onDraw;     // 每帧绘制（仅当活跃或被允许隐藏绘制）
  SceneEventFn onPause;   // 被新场景覆盖时触发（可为 NULL）
  SceneEventFn onResume;  // 重新回到栈顶时触发（可为 NULL）
  void *data;             // 场景私有数据（栈负责释放）
  GameSceneFlags flags;
  GameStack *owner; // 所属栈：压入时由 GameStack 注入，场景回调可据此切换
  bool pauseable;   // 是否允许按 ESC 调出暂停界面
};
```

**关键字段语义：**

| 字段 | 职责 | 典型取值 |
|---|---|---|
| `name` | 调试用场景名 | `"StartScene"`、`"LevelScene"` |
| `data` | 指向**场景私有数据**（`calloc` 分配，`GameStack` 释放） | `TestData*`、`StartData*` |
| `flags` | 被覆盖时是否仍更新/绘制 | 关卡 `GAME_SCENE_DRAW_WHEN_HIDDEN`，菜单 `GAME_SCENE_NONE` |
| `pauseable` | 是否允许 ESC 弹出暂停 | 关卡 `true`，菜单/暂停/过场 `false` |
| `owner` | 由栈注入，场景据此调用切换 API | `GameStackReplace(self->owner, ...)` |

**`GameSceneFlags`：**
```c
typedef enum GameSceneFlags {
  GAME_SCENE_NONE               = 0,
  GAME_SCENE_UPDATE_WHEN_HIDDEN = 1 << 0, // 被覆盖时仍更新（背景、粒子）
  GAME_SCENE_DRAW_WHEN_HIDDEN   = 1 << 1, // 被覆盖时仍绘制（暂停的半透明底层）
} GameSceneFlags;
```

### 1.3 场景私有数据（data）模式

每个场景用一个 `struct` 封装自己的状态，结构体放在 `.c` 文件内（不暴露给其它模块）：

```c
// src/scenes/scene_test.c（现有实现）
typedef struct TestData {
  const GameApp *app; // 只读引用，不拥有
  Player cat;
  SceneCamera sceneCamera; // 场景相机：由本场景持有并决定启用/禁用
  Platform platform;
  Platform platform_m;
  Rectangle source; // 当前动画帧源矩形
} TestData;
```

约定：
- `data` 由工厂函数 `calloc(1, sizeof(...))` 分配，**所有权交给栈**，场景退出时 `GameStack` 先 `onExit` 再 `free(data)`。
- 场景内通过 `GameScene *self` → `(TestData *)self->data` 取用私有数据。
- 引用 `GameApp` 时用**只读指针**（`const GameApp *app`），不拥有；若需改写（如暂停复位 `isPaused`）则持有非 const 指针。

---

## 2. 创建场景的完整流程

| 步骤 | 动作 | 输出 |
|---|---|---|
| 1 | 确定场景类型与职责（全屏/覆盖层）、是否允许暂停、需要哪些私有状态 | 设计草图 |
| 2 | 在 `include/scenes/` 新建头文件 `scene_xxx.h`：工厂函数声明 + 双重保护 | 头文件 |
| 3 | 在 `src/scenes/` 新建 `scene_xxx.c`：定义私有 `XxxData` 结构体 | 实现文件 |
| 4 | 实现生命周期回调：`XxxEnter / XxxUpdate / XxxDraw / XxxExit`（需要时加 `XxxPause / XxxResume`） | 静态函数 |
| 5 | 实现工厂函数 `XxxSceneCreate(...)`：`calloc` 场景与 data、赋值各回调与字段 | 工厂函数 |
| 6 | 接线：在既有场景中压栈/替换/弹出（或改 [`Run()`](src/game.c:9) 默认初始场景） | 集成代码 |
| 7 | 构建（`cmake --build` 自动收集新文件）并运行验证 | 通过 |

> **零构建配置**：CMake 使用 `file(GLOB_RECURSE src/*.c)`，新增 `src/scenes/scene_xxx.c` 后**无需改动 `CMakeLists.txt`**。

---

## 3. 关键接口 / 函数设计

### 3.1 `GameStack` 切换 API（全部为**延迟请求**）

| API | 用途 | 语义 |
|---|---|---|
| [`GameStackPush`](src/core/gamestack.c:179) | 压入覆盖层 | 覆盖当前栈顶（触发其 `onPause`），新场景 `onEnter` |
| [`GameStackPop`](src/core/gamestack.c:186) | 弹出覆盖层 | 栈顶 `onExit` + free，下层 `onResume` |
| [`GameStackReplace`](src/core/gamestack.c:193) | 替换栈顶 | 用于关卡/页面跳转（旧栈顶 `onExit`，新场景 `onEnter`，下层不 `onResume`） |
| [`GameStackClearTo`](src/core/gamestack.c:200) | 清空到只剩该场景 | 用于「返回主菜单」等回根操作 |
| [`GameStackRequestQuit`](src/core/gamestack.c:207) | 请求退出 | 主循环据此结束 |
| `GameStackTop / GameStackSize / GameStackEmpty / GameStackWantsQuit` | 查询 | 只读 |

**为什么必须延迟**：`GameStackUpdate` 要遍历驱动栈顶场景的 `onUpdate`，若回调中途直接 `pop`，正在迭代的场景对象会被释放 → **use-after-free**（游戏引擎最常见崩溃源）。因此所有切换 API 只入队 `pending`，由帧首 [`FlushPending`](src/core/gamestack.c:131) 统一应用，牺牲 1 帧换取彻底安全。

### 3.2 生命周期回调职责

| 回调 | 触发时机 | 职责 | 是否可空 |
|---|---|---|---|
| `onEnter` | 压入/替换后 | 加载场景资源、初始化私有数据 | 否（推荐实现） |
| `onUpdate(scene, dt)` | 每帧（活跃时） | 输入、逻辑、碰撞、相机更新 | 可空 |
| `onDraw(scene)` | 每帧（活跃时） | `GameAppBegin/End` 之间的绘制 | 可空 |
| `onExit` | 永久移出栈 | 卸载 `onEnter` 加载的资源 | 否（推荐实现） |
| `onPause` | 被新场景覆盖 | 暂停计时等 | 可空 |
| `onResume` | 重新回到栈顶 | 恢复计时等 | 可空 |

### 3.3 工厂函数约定

```c
GameScene *XxxSceneCreate(const GameApp *app, ...); // 返回栈内 malloc 的场景
```
- 名称统一 `XxxSceneCreate`，放在头文件（跨模块可见）。
- 工厂函数内 `calloc` 场景与 `data`；**不要**在工厂内加载资源——统一放 `onEnter`（资源生命周期与场景绑定）。

---

## 4. 具体实现步骤与示例代码

### 4.1 完整示例：`CountdownScene`（过场倒计时）

一个可独立运行的全屏过场场景，演示**完整生命周期**（进入/更新/绘制/退出）与**场景切换**。倒计时结束后自动 `Replace` 到指定场景。

**步骤 1：头文件 `include/scenes/scene_countdown.h`**

```c
#ifndef SCENE_COUNTDOWN_H
#define SCENE_COUNTDOWN_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"

// 创建倒计时过场场景：显示 3-2-1 倒计时后自动跳转到 next 场景。
// 参数 next：倒计时结束后要切换到的场景（由调用方创建，所有权转移给本场景）。
GameScene *CountdownSceneCreate(const GameApp *app, GameScene *next);

#endif // SCENE_COUNTDOWN_H
```

**步骤 2：实现 `src/scenes/scene_countdown.c`**

```c
#include "scenes/scene_countdown.h"
#include <raylib.h>
#include <stdlib.h>

// 场景私有数据：栈持有并负责释放
typedef struct CountdownData {
  const GameApp *app; // 只读引用，不拥有
  float remaining;    // 剩余秒数
  GameScene *next;    // 倒计时结束后要切换到的场景（所有权已转移）
} CountdownData;

#define COUNTDOWN_SECONDS 3.0f

static void CountdownEnter(GameScene *self) {
  CountdownData *d = (CountdownData *)self->data;
  d->remaining = COUNTDOWN_SECONDS;
  // 若本场景有专属资源，在此 LoadTexture / LoadSound
}

static void CountdownUpdate(GameScene *self, float dt) {
  CountdownData *d = (CountdownData *)self->data;
  d->remaining -= dt;
  if (d->remaining <= 0.0f) {
    // 延迟请求：下一帧帧首才真正替换，回调内调用是安全的
    GameStackReplace(self->owner, d->next);
  }
}

static void CountdownDraw(GameScene *self) {
  CountdownData *d = (CountdownData *)self->data;
  // 全屏覆盖：先铺不透明背景，再居中绘制倒计时数字
  DrawRectangle(0, 0, d->app->logicWidth, d->app->logicHeight, BLACK);
  int seconds = (int)ceilf(d->remaining);
  const char *text = TextFormat("%d", seconds);
  int size = 96;
  DrawText(text,
           (d->app->logicWidth - MeasureText(text, size)) / 2,
           (d->app->logicHeight - size) / 2,
           size, WHITE);
}

static void CountdownExit(GameScene *self) {
  (void)self;
  // 卸载 onEnter 中加载的资源（如有）
}

GameScene *CountdownSceneCreate(const GameApp *app, GameScene *next) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  CountdownData *data = (CountdownData *)calloc(1, sizeof(CountdownData));
  data->app = app;
  data->next = next; // 转移所有权给本场景

  scene->name = "CountdownScene";
  scene->data = data;
  scene->flags = GAME_SCENE_NONE;
  scene->pauseable = false; // 过场不允许暂停
  scene->onEnter = CountdownEnter;
  scene->onUpdate = CountdownUpdate;
  scene->onDraw = CountdownDraw;
  scene->onExit = CountdownExit;
  // onPause / onResume 本场景不需要，保持 NULL
  return scene;
}
```

> ⚠️ 所有权陷阱：`next` 的所有权转移给了 `CountdownScene`。若本场景被异常 `Pop`（而非正常 `Replace`），`next` 会泄漏。解法：在 `CountdownExit` 中释放尚未消费的 `next`，或约定本场景只被 `Replace` 使用。见 §7。

### 4.2 覆盖层场景示例：半透明遮罩（参考 [`scene_pause.c`](src/scenes/scene_pause.c:76)）

覆盖层通常配合下层场景的 `GAME_SCENE_DRAW_WHEN_HIDDEN` 使用：

```c
static void PauseDraw(GameScene *self) {
  PauseData *d = (PauseData *)self->data;
  // 半透明遮罩盖在下层关卡之上
  DrawRectangle(0, 0, d->app->logicWidth, d->app->logicHeight,
                Fade(BLACK, 0.55f));
  // ... 标题与按钮（配合 MenuNav + raygui）...
}
```

**关键约束**（与主循环的暂停状态机协作）：
- 下层关卡 `flags = GAME_SCENE_DRAW_WHEN_HIDDEN`，被覆盖时仍绘制 → 成为半透明底。
- 暂停场景 `pauseable = false`，`name` 见上；ESC 进入/退出由 [`Run()`](src/game.c:26) 按 `app.isPaused` 状态机统一处理，暂停场景**不要**自己响应 ESC（避免同一事件同帧弹出）。
- 「返回主菜单」用 `GameStackClearTo(self->owner, StartSceneCreate(d->app))` 清空栈。

### 4.3 状态切换接线示例（菜单 → 关卡）

```c
// src/scenes/scene_start.c（现有实现的模式）
case START_ACTION_PLAY:
  // Replace：从菜单进入关卡，栈中不残留菜单
  GameStackReplace(self->owner, LevelSceneCreate(d->app, 1));
  break;
case START_ACTION_QUIT:
  GameStackRequestQuit(self->owner);
  break;
```

### 4.4 关卡场景骨架：`LevelScene`（多关卡复用）

```c
// include/scenes/scene_level.h
#ifndef SCENE_LEVEL_H
#define SCENE_LEVEL_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"

// 通用关卡场景：通过 levelId 参数化，一套代码承载多关。
// levelId 决定平台布局、单词表与关卡类型（后续由 systems/level_flow 驱动）。
GameScene *LevelSceneCreate(const GameApp *app, int levelId);

#endif // SCENE_LEVEL_H
```

```c
// src/scenes/scene_level.c
#include "scenes/scene_level.h"
#include "entities/player.h"
#include "tools/camera.h"
#include <stdlib.h>

typedef struct LevelData {
  const GameApp *app;
  int levelId;
  Player cat;
  SceneCamera sceneCamera;
  Platform platform;
  Rectangle source; // 当前动画帧源矩形
} LevelData;

static void LevelEnter(GameScene *self) {
  LevelData *d = (LevelData *)self->data;
  d->cat = (Player){0};
  InitPlayer(&d->cat);
  d->platform = (Platform){0};
  InitJumpPlatforms(&d->platform, (Vector2){100, 350}, SMALL);
  InitSceneCamera(&d->sceneCamera, d->app->logicWidth, d->app->logicHeight,
                  true, CAMERA_FOLLOW_CENTER);
}

static void LevelUpdate(GameScene *self, float dt) {
  LevelData *d = (LevelData *)self->data;
  UpdatePlayer(&d->cat, dt);
  d->cat.isOnTheGround = false;
  PlayerCollision(&d->cat, &d->platform, dt);
  GroundCollision(&d->cat);
  SetCameraTarget(&d->sceneCamera, d->cat.position);
  UpdateSceneCamera(&d->sceneCamera, dt);
  d->source = AnimationUpdate(&d->cat.animations[d->cat.playerAnimationState], dt);
}

static void LevelDraw(GameScene *self) {
  LevelData *d = (LevelData *)self->data;
  BeginSceneCamera(&d->sceneCamera);
  DrawPlatform(&d->platform);
  DrawPlayer(&d->cat, d->source);
  EndSceneCamera(&d->sceneCamera);
}

static void LevelExit(GameScene *self) {
  LevelData *d = (LevelData *)self->data;
  UnloadTexture(d->cat.idleTexture);
  UnloadTexture(d->cat.runTexture);
  UnloadTexture(d->cat.jumpTexture);
  UnloadTexture(d->cat.sleepTexture);
  if (d->platform.platformTexture.id != 0)
    UnloadTexture(d->platform.platformTexture);
}

GameScene *LevelSceneCreate(const GameApp *app, int levelId) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  LevelData *data = (LevelData *)calloc(1, sizeof(LevelData));
  data->app = app;
  data->levelId = levelId;

  scene->name = "LevelScene";
  scene->data = data;
  scene->flags = GAME_SCENE_DRAW_WHEN_HIDDEN; // 暂停时仍绘制作为半透明底
  scene->pauseable = true;                    // 关卡允许 ESC 暂停
  scene->onEnter = LevelEnter;
  scene->onUpdate = LevelUpdate;
  scene->onDraw = LevelDraw;
  scene->onExit = LevelExit;
  return scene;
}
```

---

## 5. 涉及的文件路径

| 文件 | 职责 |
|---|---|
| `include/core/gamestack.h` | `GameScene` / `GameStack` 定义与 API 声明 |
| `src/core/gamestack.c` | 场景栈实现（延迟请求队列、生命周期） |
| `include/core/gameapp.h`、`src/core/gameapp.c` | 框架：窗口 / 渲染目标 / Present / 暂停状态 |
| `src/game.c` | [`Run()`](src/game.c:9) 主循环，默认压入 [`StartSceneCreate`](src/scenes/scene_start.c:119) |
| `include/scenes/scene_xxx.h`、`src/scenes/scene_xxx.c` | **新场景的落点**（你创建的） |
| `include/scenes/scene_start.h`、`src/scenes/scene_start.c` | 开始菜单（作为接入/跳转示例） |
| `include/scenes/scene_test.h`、`src/scenes/scene_test.c` | 测试关卡（骨架验证，后续拆分为 `LevelScene`） |
| `include/scenes/scene_pause.h`、`src/scenes/scene_pause.c` | 暂停覆盖层（覆盖层示例） |
| `include/tools/menu.h`、`src/tools/menu.c` | [`MenuNav`](include/tools/menu.h:22) 键盘导航 |
| `include/tools/camera.h`、`src/tools/camera.c` | 场景相机（关卡用） |
| `include/tools/animation.h` | inline 动画（关卡玩家用） |
| `include/entities/player.h`、`src/entities/player.c` | 玩家实体 |
| `include/entities/platform.h`、`src/entities/platform.c` | 平台实体 |
| `include/core/game_config.h` | 全局常量（`GAME_SCALE`，待扩展逻辑分辨率/地面 Y） |
| `CMakeLists.txt` | `GLOB_RECURSE` 自动收集新文件，**无需修改** |

---

## 6. 与现有模块的集成方式

### 6.1 让新场景成为初始场景
- 修改 [`Run()`](src/game.c:15)：`GameStackPush(stack, StartSceneCreate(&app))` → `GameStackPush(stack, XxxSceneCreate(&app))`。
- 注意 `GameAppInit` 必须在创建任何场景**之前**调用（`LoadTexture` 依赖 `InitWindow`）。

### 6.2 场景之间的跳转
- 通过 `self->owner` + 延迟 API 完成，见 §3.1 与 §4.3。
- 需要把参数（如 `levelId`、回调目标）传给新场景时，经工厂函数参数传入，存进 `data`。
- **带过渡动画的跳转**：需要转场时，先 `Replace` 到通用过渡场景
  [`TransitionSceneCreate`](src/scenes/scene_transition.c:96)，由它在「淡入-保持-淡出」遮罩动画结束后
  自动 `Replace` 到目标场景（`next` 所有权转移给过渡场景；过渡场景
  `pauseable=false`、`flags=GAME_SCENE_NONE`，全屏不透明过渡）：
  - 菜单→关卡：`GameStackReplace(owner, TransitionSceneCreate(app, LevelSceneCreate(app, 1)))`
  - 关卡→关卡：`GameStackReplace(owner, TransitionSceneCreate(app, LevelSceneCreate(app, 2)))`
  - 关卡→菜单：`GameStackReplace(owner, TransitionSceneCreate(app, StartSceneCreate(app)))`

### 6.3 与 UI 集成（菜单 / 按钮）
- 键盘导航统一用 [`MenuNav`](include/tools/menu.h:22)：`MenuNavInit(&d->nav, count)` → 每帧 `MenuNavUpdate(&d->nav)` 返回 `MENU_ACTION_CONFIRM/BACK`。
- raygui 按钮：`GuiButton(rec, label)` + 键盘选中 `GuiSetState(STATE_FOCUSED)` + 鼠标悬停同步 `nav.selected`。
- 选中项切换播放 `PlaySound(d->app->uiSound)`（音效由 `GameApp` 统一持有）。

### 6.4 与相机 / 实体集成
- 关卡场景持有自己的 [`SceneCamera`](include/tools/camera.h:27)，`onEnter` 用 `InitSceneCamera` 初始化并选定 `CameraFollowMode`（固定镜头 `CAMERA_FOLLOW_NONE`；平台跳跃 `CAMERA_FOLLOW_CENTER` 等，按玩法选择）。
- 绘制时 `BeginSceneCamera` / `EndSceneCamera` 包住实体绘制。

### 6.5 资源生命周期
- 场景专属资源：`onEnter` 加载、`onExit` 卸载。
- 框架级资源（图标、`RenderTexture`、音频设备）：`GameAppClose` 统一释放，场景不要碰。
- 多层共享资源（如暂停也要用的字体）：放入 `GameApp` 共享，或后续引入 `AssetManager`（见架构文档扩展点）。

---

## 7. 异常处理与边界情况

| # | 边界 / 异常 | 处理方式 |
|---|---|---|
| 1 | 场景回调内多次调用切换 API | 全部入队，帧首按序 flush，安全；但要注意顺序语义（见 §9 FAQ） |
| 2 | 空栈（`GameStackSize == 0`） | [`GameStackUpdate`](src/core/gamestack.c:236) 直接返回，主循环因栈空结束 |
| 3 | `data == NULL` 或工厂失败 | `calloc` 失败返回 NULL 时，工厂应返回 NULL 并让调用方判空；现有代码用 `calloc` 不判空（低概率），新代码建议判空 |
| 4 | `next` 等转移所有权对象未被消费 | 正常路径用 `Replace` 消费；异常路径（被 `Pop`）需在 `onExit` 释放，见 §4.1 注释 |
| 5 | 资源加载失败（贴图/图片） | `LoadTexture` 检查 `id != 0`、`LoadImage` 检查 `data != NULL`，失败降级（置零尺寸/跳过绘制），参考 [`platform.c`](src/entities/platform.c:33) |
| 6 | ESC 与暂停竞态 | 暂停场景不响应 ESC，由主循环 `app.isPaused` 状态机统一处理，避免同帧弹入弹出 |
| 7 | `onExit` 未实现导致资源泄漏 | 场景在 `onEnter` 加载的每个资源都必须在 `onExit` 卸载，一一配对 |
| 8 | 覆盖层 `DRAW_WHEN_HIDDEN` 未设置 | 暂停时下层不绘制 → 遮罩下是空白，需给下层场景加 `GAME_SCENE_DRAW_WHEN_HIDDEN` |
| 9 | `Replace` 到空栈 | [`ReplaceImmediate`](src/core/gamestack.c:99) 自动退化为 `Push`，安全 |
| 10 | `ClearTo` 传入栈内已有对象 | 设计上要求传入**新建**场景，传栈内对象会导致双重释放（见 [`gamestack.c`](src/core/gamestack.c:116) 注释） |

---

## 8. 性能与安全考量

### 8.1 安全性（防崩溃）
- **延迟请求机制**是核心防线：场景回调中切换场景是安全的（use-after-free 已被架构消除）。
- **禁止危险函数**：不使用 `strcpy/strcat/sprintf/system/popen`；字符串用 raylib `TextFormat`（见 [`code_style.md`](code_style.md) §9）。
- **内存安全**：`realloc` 返回值必须检查（安全审计 M1）；`malloc/calloc` 与 `free` 配对；不要手动 `free(data)`（栈负责）。
- **除零防护**：任何除法前校验除数（如动画 `frameCount > 0`，审计 M3）。
- **链接一致性**：头文件声明与 `.c` 实现必须同名同签名（审计 M2）。

### 8.2 性能
- **资源不重复加载**：场景被覆盖/恢复（`onPause/onResume`）不重新加载资源，只在 `onEnter/onExit` 加载/卸载。
- **避免每帧分配**：不要在每个 `onUpdate` 中 `malloc`；初始化时一次性分配，退出时释放。
- **固定分辨率渲染**：所有绘制在 640×480 渲染目标内完成，Present 统一缩放；场景无需关心窗口尺寸。
- **60FPS**：`SetTargetFPS(60)`，`dt` 用 `GetFrameTime()`；暂停时 `dt = 0`（由主循环处理）。
- 场景回调保持轻量：把耗时计算放初始化或必要时做，避免帧内阻塞。

### 8.3 资源路径
- 资源统一经 `GetApplicationDirectory()` + `TextFormat("%sassets/...", ...)` 定位，不要硬编码绝对路径（安全审计 M4）。

---

## 9. 常见问题与排查建议

| # | 现象 | 原因 | 排查 / 修复 |
|---|---|---|---|
| 1 | 新增场景编译找不到函数 | 头文件未加入 `#include` 或工厂函数加了 `static` | 检查头文件声明与 `.c` 实现签名一致、工厂函数非 `static` |
| 2 | 场景切换"晚了一帧" | 延迟请求机制的正常表现 | 属预期；如需即时可在下一帧 flush 后观察 |
| 3 | ESC 弹暂停一闪而过 | 暂停场景自身响应了 ESC 或主循环状态机重复处理 | 暂停场景不要检测 ESC，交给 [`Run()`](src/game.c:26) 统一处理 |
| 4 | 暂停时下层画面空白 | 下层场景缺少 `GAME_SCENE_DRAW_WHEN_HIDDEN` | 给下层关卡场景加该 flag |
| 5 | 返回主菜单后栈里还有残留 | 用 `Pop` 而非 `ClearTo` | 回根操作用 `GameStackClearTo(self->owner, StartSceneCreate(...))` |
| 6 | 资源泄漏（纹理/音频未释放） | `onExit` 未卸载 `onEnter` 加载的资源 | 逐一配对检查；`UnloadTexture/UnloadSound` 补齐 |
| 7 | 玩家在场景间状态不重置 | `data` 复用旧值 | `onEnter` 中重新零初始化并 `InitXxx`（如 `d->cat = (Player){0}; InitPlayer(...)`） |
| 8 | 相机画面跳动/错位 | 相机未初始化或模式选错 | `InitSceneCamera` 时选对 `CameraFollowMode`；地面/分辨率魔法数统一到 `game_config.h` |
| 9 | 鼠标点击按钮无反应 | 使用了窗口像素坐标做命中检测 | 所有 UI 用逻辑坐标，鼠标坐标由 `GameAppBegin` 内 `ApplyViewportScale` 已变换 |
| 10 | 构建后资源找不到 | assets 未复制到输出目录 | 检查 `CMakeLists.txt` 的 `POST_BUILD copy_directory` 是否生效 |
| 11 | 场景切换后崩溃（use-after-free） | 直接调用内部 `PushImmediate` 或在回调中改栈 | 只用公开延迟 API（`GameStackPush/...`），不要在 `onUpdate` 内直接操作 `stack->scenes` |
| 12 | 无法编译警告过多 | 新代码风格不合规 | 对照 [`code_style.md`](code_style.md) §8/§11 检查并开启 `-Wall -Wextra -Werror` |

---

## 10. 创建场景检查清单

- [ ] 头文件使用 `#ifndef + #define + #pragma once` 双重保护
- [ ] 头文件只含类型与声明，工厂函数 `XxxSceneCreate` 非 `static`
- [ ] `.c` 内定义私有 `XxxData`，工厂 `calloc` 场景与 data
- [ ] `onEnter` 加载资源并零初始化私有数据；`onExit` 一一卸载
- [ ] `name` / `flags` / `pauseable` 设置正确（关卡 `true` + `DRAW_WHEN_HIDDEN`，菜单/过场 `false`）
- [ ] 场景切换只用公开延迟 API，经 `self->owner`
- [ ] 覆盖层配合下层 `GAME_SCENE_DRAW_WHEN_HIDDEN` + 半透明遮罩
- [ ] 资源加载判空、`realloc` 检查、除零防护、公共 API 判空
- [ ] 不使用危险函数；不用 `strings.c` 死代码模块
- [ ] 魔法数具名、中文注释说明"为什么"
- [ ] 构建通过、运行验证场景进入/退出/覆盖/恢复均正常
