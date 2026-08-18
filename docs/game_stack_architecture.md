# 游戏栈（Game Stack）架构方案

> 目标：将当前耦合在 [`Run()`](src/game.c:8) 中的单关卡逻辑，重构为「场景（Scene）+ 场景栈（Stack）」驱动的框架，为未来的**开始界面、多关卡、暂停、结算、设置、过场**等场景做好扩展准备。

---

## 1. 背景与现状

当前 [`Run()`](src/game.c:8) 在一个函数里完成了以下所有职责，彼此强耦合：

- 窗口 / 渲染目标（`RenderTexture`）初始化与全屏切换
- 玩家与平台的创建、更新、碰撞
- 每帧绘制到固定分辨率渲染目标，再等比缩放到窗口
- 主循环与资源卸载

**问题**：
1. 无法叠加 UI 层（如暂停菜单需要「游戏画面 + 半透明遮罩 + 菜单」三层同屏）。
2. 无法做场景切换（开始界面 → 关卡 → 结算）的状态管理。
3. 无法复用资源加载/卸载的通用流程。
4. `Run()` 会随功能增多无限膨胀。

**方案**：引入经典的 **Scene Stack（场景栈）** 架构。栈顶场景为「唯一活跃」场景，被压入的新场景可覆盖在旧场景之上（例如暂停层盖在关卡之上），弹出后自动恢复下层场景。

```
┌──────────────────────────────┐
│       PauseScene             │  ← 活跃：更新 + 绘制
├──────────────────────────────┤
│       LevelScene             │  ← 被覆盖：暂停，仅按需绘制（半透明底）
├──────────────────────────────┤
│       MenuScene              │  ← 栈底
└──────────────────────────────┘
```

---

## 2. 总体架构（分层）

```
┌────────────────────────────────────────────┐
│ 场景层  scenes/                             │
│  MenuScene / LevelScene / PauseScene /     │
│  GameOverScene / SettingsScene / ...       │
├────────────────────────────────────────────┤
│ 场景核心  core/gamestack                    │
│ GameScene 定义 + GameStack + 延迟请求队列     │
├────────────────────────────────────────────┤
│ 游戏对象层  src/                              │
│  Player / Platform / Animation / Words     │
├────────────────────────────────────────────┤
│ 工具层  core/                               │
│  Timer / String / GenRandom                │
└────────────────────────────────────────────┘
```

**依赖方向**：上层依赖下层，下层绝不反向依赖上层。场景层通过「创建函数」返回 [`GameScene`](include/core/gamestack.h)，游戏对象层与场景栈核心互不知晓对方细节。

---

## 3. 核心概念一：`GameScene`（场景）

场景 = 一个**可独立运行的最小游戏单元**（菜单、关卡、暂停、结算都算场景）。每个场景通过函数指针声明自己的生命周期行为，并通过 `data` 持有私有数据。

```c
// include/core/gamestack.h（规划稿）
typedef struct GameScene GameScene;

typedef void (*SceneUpdateFn)(GameScene *scene, float dt);
typedef void (*SceneDrawFn)(GameScene *scene);
typedef void (*SceneEventFn)(GameScene *scene);

typedef enum GameSceneFlags {
  GAME_SCENE_NONE               = 0,
  GAME_SCENE_UPDATE_WHEN_HIDDEN = 1 << 0, // 被覆盖时仍更新（背景、粒子）
  GAME_SCENE_DRAW_WHEN_HIDDEN   = 1 << 1, // 被覆盖时仍绘制（暂停的半透明底层）
} GameSceneFlags;

struct GameScene {
  const char *name;      // 场景名（调试用）
  SceneEventFn  onEnter; // 进入：加载本场景资源
  SceneEventFn  onExit;  // 永久离开：卸载本场景资源
  SceneUpdateFn onUpdate;// 每帧更新（仅当活跃或被允许隐藏更新）
  SceneDrawFn   onDraw;  // 每帧绘制（仅当活跃或被允许隐藏绘制）
  SceneEventFn  onPause; // 被新场景覆盖时触发
  SceneEventFn  onResume;// 重新回到栈顶时触发
  void *data;            // 场景私有数据
  GameSceneFlags flags;
};
```

**约定**：
- 场景用 `*SceneCreate(...)` 工厂函数创建（栈内 malloc），销毁由 `GameStack` 统一负责（先 `onExit` 再 `free`）。
- `onExit` 只在该场景被**永久移出**栈时调用；被覆盖只是 `onPause`，不会销毁，因此**覆盖/恢复的开销极低**。

---

## 4. 核心概念二：`GameStack`（场景栈）

`GameStack` 管理场景的入栈/出栈/替换，并**通过延迟请求队列**确保栈在任意回调中都不会被中途修改。

```c
// include/core/gamestack.h（规划稿）
typedef struct GameStack GameStack;

GameStack  *GameStackCreate(void);
void        GameStackDestroy(GameStack *stack);

void        GameStackPush(GameStack *stack, GameScene *scene);  // 压入（覆盖当前）
void        GameStackPop(GameStack *stack);                     // 弹出（恢复下层）
void        GameStackReplace(GameStack *stack, GameScene *scene);// 替换栈顶（关卡跳转）
void        GameStackClearTo(GameStack *stack, GameScene *scene);// 清空到仅剩该场景

GameScene  *GameStackTop(GameStack *stack);   // 当前活跃场景
int         GameStackSize(GameStack *stack);
bool        GameStackEmpty(GameStack *stack);

void        GameStackUpdate(GameStack *stack, float dt); // 帧首 flush 请求 + 驱动场景
void        GameStackDraw(GameStack *stack);             // 按“底层→栈顶”顺序绘制
void        GameStackRequestQuit(GameStack *stack);      // 请求退出游戏
bool        GameStackWantsQuit(GameStack *stack);
```

### 4.1 延迟请求机制（关键设计）

场景的 `onUpdate` 内**直接调用** `GameStackPush/Pop/...` 不会立即改栈，而是写入待处理队列（`pending` 数组），由 `GameStackUpdate` 在**下一帧帧首统一 flush** 到真实栈中。

**为什么必须这样做**：`GameStackUpdate` 需要遍历栈顶场景驱动其 `onUpdate`，若回调中途 `pop` 掉栈顶，会导致迭代中的对象被释放（use-after-free），是游戏引擎最常见的崩溃源之一。

```text
场景 onUpdate 内调用 GameStackPop()
   └─> 写入 pending 队列（安全，不改真实栈）
下一帧 GameStackUpdate 帧首：flush pending → 真实栈执行 pop
   └─> 被 pop 的场景 onExit + free；下层场景 onResume
```

该方案牺牲 1 帧延迟，换取**彻底的安全性与代码简洁性**，对 60FPS 游戏完全无感。

### 4.2 场景生命周期状态机

```
                GameStackPush(new)
   [Dormant] ───────────────────────► [Active]（新场景 onEnter）
      ▲                                  │
      │ GameStackClearTo / 栈清空          │ GameStackPush(覆盖层)
      │ (onExit + free)                   ▼
   [Destroyed]                        [Paused]（onPause；下层）
      ▲                                  │
      │                                  │ GameStackPop(覆盖层)
      └──────── onExit + free ◄──────────┘ (onResume)
```

- `Active`：唯一执行 `onUpdate/onDraw`。
- `Paused`：不更新；是否绘制由 `GAME_SCENE_DRAW_WHEN_HIDDEN` 决定（暂停场景底下那一层用它显示半透明游戏画面）。
- 栈空或收到 `RequestQuit` 时，主循环结束。

---

## 5. 主循环改造（框架层）

[`Run()`](src/game.c:8) 收敛为「框架初始化 + 驱动 GameStack」，平台/渲染细节下沉到框架：

```c
void Run() {
  // ① 框架初始化：窗口、图标、固定分辨率 RenderTexture、音频、60FPS
  //    （全屏切换、RenderTexture 缩放绘制抽成通用 GameApp 函数）
  GameApp app = GameAppInit(640, 480, "CatET");

  // ② 创建栈并压入初始场景（开始界面）
  GameStack *stack = GameStackCreate();
  GameStackPush(stack, MenuSceneCreate(&app));

  // ③ 主循环只做三件事：事件 → 更新 → 绘制
  while (!WindowShouldClose() && !GameStackWantsQuit(stack)) {
    GameAppPollGlobalInput(&app);        // 全屏切换等全局按键
    float dt = GetFrameTime();
    GameStackUpdate(stack, dt);          // 帧首 flush 请求 + 驱动栈顶场景
    GameStackDraw(stack);                // 底层→栈顶依次绘制，最后缩放输出
  }

  // ④ 清理：销毁场景栈 → 框架释放资源 → 关窗
  GameStackDestroy(stack);
  GameAppClose(&app);
}
```

`Run()` 的职责收敛后，新增任何场景都**不需要再动主循环**。

---

## 6. 渲染与缩放的处理

现有「固定分辨率渲染目标 + 等比缩放居中 + 黑边」逻辑（[`Run()`](src/game.c:56)）提升为框架级通用能力，供所有场景复用：

```c
// include/core/gameapp.h（规划稿）
void GameAppBegin(GameApp *app);       // BeginTextureMode(target)
void GameAppEnd(GameApp *app);         // EndTextureMode()
void GameAppPresent(GameApp *app);     // 缩放绘制到窗口 + BeginDrawing/EndDrawing
```

- 场景的 `onDraw` 只需在 `GameAppBegin/End` 之间画到逻辑坐标系，无需关心窗口尺寸。
- 缩放、居中、黑边、全屏切换全部集中在 `GameAppPresent`，一处改动全局生效。

---

## 7. 目录与文件规划

```
include/
  core/
    gamestack.h      # GameScene + GameStack（扩展现有占位）
    gameapp.h        # 框架：窗口/RenderTexture/Present/音频（新）
  scenes/
    scene_menu.h     # 开始界面
    scene_level.h    # 通用关卡（通过关卡 ID 参数化，可复用于多关）
    scene_pause.h    # 暂停（覆盖层）
    scene_gameover.h # 结算
    scene_settings.h # 设置（可选）
src/
  core/
    gamestack.c      # 实现（扩展现有占位）
    gameapp.c        # 框架实现（新）
  scenes/
    scene_menu.c
    scene_level.c
    scene_pause.c
    scene_gameover.c
    scene_settings.c
  game.c             # Run() 收敛为入口，仅初始化框架 + 压入菜单场景
```

> `CMakeLists.txt` 已使用 `file(GLOB_RECURSE src/*.c)`，新增 `src/scenes/` 下文件**无需改动构建脚本**。

---

## 8. 场景示例

### 8.1 开始界面 `MenuScene`

- `onEnter`：加载菜单背景/标题字体。
- `onUpdate`：检测「开始游戏」→ `GameStackReplace(stack, LevelSceneCreate(levelId=1))`（替换而非压栈，避免从结算返回时栈里残留菜单）。
- `onDraw`：`GameAppBegin` 后绘制标题、菜单项；`GameAppEnd`。

### 8.2 通用关卡 `LevelScene`（多关卡复用）

- `data` 指向 `LevelData { int levelId; Player cat; Platform platform; ... }`。
- 工厂函数 `LevelSceneCreate(int levelId)` 按关卡 ID 初始化平台布局 / 单词表（`assets/words/CET4.txt`、`CET6.txt`），**一套代码跑所有关卡**。
- `onUpdate`：处理 `ESC` → `GameStackPush(stack, PauseSceneCreate(...))` 覆盖暂停。
- `onExit`：卸载本关资源。

### 8.3 暂停 `PauseScene`（覆盖层）

- 创建时，下层 `LevelScene` 触发 `onPause`；因 `LevelScene` 带 `GAME_SCENE_DRAW_WHEN_HIDDEN`，其画面仍绘制为暂停菜单的半透明底。
- 「继续」→ `GameStackPop(stack)`，下层 `onResume` 恢复计时。
- 「退出关卡」→ 先 `GameStackPop`（移除暂停），再 `GameStackReplace(LevelScene, MenuSceneCreate())` 回到主菜单。

---

## 9. 资源管理策略

- **场景级资源**（贴图、字体、音效）：`onEnter` 加载、`onExit` 卸载，生命周期与场景绑定，栈内覆盖/恢复不重复加载。
- **框架级资源**（窗口图标、`RenderTexture`、音频设备）：由 `GameApp` 统一持有与释放。
- 多层场景都需要的资源（如暂停也要用到的字体）：放入 `GameApp` 共享，或引入后续的 `AssetManager` 引用计数（见扩展点）。

---

## 10. 未来扩展点（预留接口）

| 扩展方向 | 落点 | 说明 |
|---|---|---|
| 过场/转场动画 | `TransitionScene` 作为覆盖层，淡入淡出后执行真实切换 | 复用 Push/Pop 机制 |
| 资源管理器 | `AssetManager` + 引用计数 | 替代「onEnter/onExit 各自加载」的朴素策略 |
| 音频管理 | `AudioManager`（BGM 跨场景续播） | 独立于场景栈 |
| UI 系统 | `widget.h`（按钮/文本/焦点） | 菜单与暂停共用，避免重复手写 `IsMouseButtonPressed` 判断 |
| 存档/设置 | `SettingsScene` + `save.json` | 通过 `Replace` 进入 |
| 单帧输入缓冲 | 场景栈核心增加事件队列 | 需要时再引入 |

---

## 11. 实施路线（迁移步骤）

1. **落地核心**：实现 [`gamestack.h`](include/core/gamestack.h) / [`gamestack.c`](src/core/gamestack.c)（`GameScene`、`GameStack`、延迟请求队列）+ [`gameapp.h`](include/core/gameapp.h) / `gameapp.c`（窗口、`RenderTexture`、`Present`）。
2. **迁移现有玩法**：把 [`Run()`](src/game.c) 中的玩家/平台逻辑搬进 `LevelScene`（`LevelSceneCreate(1)`），用 `GameStack` 驱动，行为保持不变（回归测试点）。
3. **加暂停**：实现 `PauseScene` 覆盖层，验证 `onPause/onResume` 与半透明底层绘制。
4. **加开始界面**：实现 `MenuScene` 为默认初始场景，验证 `Replace` 跳关与返回。
5. **多关卡化**：`LevelSceneCreate(levelId)` 参数化平台与单词表，接入 `CET4/CET6` 词库切换。
6. **打磨**：结算场景、设置、转场动画按需推进。

