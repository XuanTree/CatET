#include "game.h"
#include "core/gameapp.h"
#include "core/gamestack.h"
#include "scenes/scene_test.h"

// 逻辑分辨率固定不变，窗口放大时通过 RenderTexture 等比缩放，画面不变糊
#define LOGIC_WIDTH 640
#define LOGIC_HEIGHT 480

void Run() {
  // 框架初始化：窗口、图标、音频、固定分辨率渲染目标
  GameApp app = GameAppInit(LOGIC_WIDTH, LOGIC_HEIGHT, "CatET");

  // 创建场景栈并压入初始场景（测试场景，保持原玩法行为不变）
  GameStack *stack = GameStackCreate();
  GameStackPush(stack, TestSceneCreate(&app));

  // 主循环：事件 → 更新 → 绘制
  while (!WindowShouldClose() && !GameStackWantsQuit(stack)) {
    GameAppPollGlobalInput(); // F11 / Alt+Enter 全屏切换

    float dt = GetFrameTime();
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
