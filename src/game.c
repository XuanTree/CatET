#include "game.h"
#include "core/gameapp.h"
#include "core/gamestack.h"

// 逻辑分辨率固定不变，窗口放大时通过 RenderTexture 等比缩放，画面不变糊
#define LOGIC_WIDTH 640
#define LOGIC_HEIGHT 480

void Run() {
  // 框架初始化：窗口、图标、音频、固定分辨率渲染目标
  GameApp app = GameAppInit(LOGIC_WIDTH, LOGIC_HEIGHT, "CatET");

  // 创建场景栈并压入初始场景：开始菜单（Play 后经栈替换进入测试关卡）
  GameStack *stack = GameStackCreate();
  GameStackPush(stack, StartSceneCreate(&app));

  // 主循环：事件 → 更新 → 绘制
  while (!WindowShouldClose() && !GameStackWantsQuit(stack)) {
    GameAppPollGlobalInput(); // F11 / Alt+Enter 全屏切换

    // 暂停：ESC 在「游戏场景」与「暂停界面」之间切换（isPaused 状态机）。
    // 仅当栈顶场景允许暂停（pauseable）时才能进入暂停；开始界面等菜单
    // pauseable=false，无法调出暂停画面。ESC 进入/退出统一在此处理，
    // 暂停场景自身不再响应 ESC，避免同一 ESC 事件导致刚推入的暂停界面
    // 在同帧内被立即弹出（一闪而过）。
    if (IsKeyPressed(KEY_ESCAPE)) {
      if (!app.isPaused) {
        GameScene *top = GameStackTop(stack);
        if (top && top->pauseable) {
          app.isPaused = true;
          GameStackPush(stack, PauseSceneCreate(&app));
        }
      } else {
        app.isPaused = false;
        GameStackPop(stack);
      }
    }

    // 暂停时冻结逻辑时间（场景切换请求仍每帧 flush）
    float dt = app.isPaused ? 0.0f : GetFrameTime();
    app.runTime += dt;          // 全局运行计时：暂停时不计，供关卡 HUD 显示
    GameStackUpdate(stack, dt); // 帧首 flush 切换请求 + 驱动栈顶场景

    // 统一绘制：先绘制到固定分辨率渲染目标，再等比缩放到窗口
    GameAppBegin(&app);
    GameStackDraw(stack);
    GameAppEnd(&app);
    GameAppPresent(&app);
  }

  // 清理：销毁场景栈（各场景 onExit + 释放）→ 框架释放资源 → 关窗
  GameStackDestroy(stack);
  GameAppClose(&app);
}
