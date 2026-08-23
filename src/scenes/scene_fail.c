#include "scenes/scene_fail.h"
#include "scenes/scene_start.h"
#include "tools/menu.h"
#include "tools/raygui.h"
#include <raylib.h>
#include <stdlib.h>

// ── 失败界面交互结果 ───────────────────────────────────────────────
typedef enum FailAction {
  FAIL_ACTION_NONE = 0,
  FAIL_ACTION_QUIT_TO_MENU, // 回到菜单（清空栈到开始界面）
  FAIL_ACTION_QUIT,         // 退出游戏
} FailAction;

// 失败界面选项定义（顺序与 MenuNav.selected 索引一一对应）。
// 玩家在失败后只能选择「回到菜单」或「退出游戏」。
static const char *const kFailLabels[] = {"Back to Menu", "Quit"};
static const FailAction kFailActions[] = {FAIL_ACTION_QUIT_TO_MENU,
                                          FAIL_ACTION_QUIT};
#define FAIL_ITEM_COUNT 2

// 场景私有数据：栈持有并负责释放
typedef struct FailData {
  GameApp *app;      // 引用（不拥有）；回到菜单时需传给 StartSceneCreate
  MenuNav nav;       // 键盘导航（W/S/↑↓ 移动，Z 确认）
  FailAction action; // Draw/Update 阶段由按钮产生，Update 消费执行
} FailData;

static void FailEnter(GameScene *self) {
  FailData *d = (FailData *)self->data;
  MenuNavInit(&d->nav, FAIL_ITEM_COUNT);
}

static void FailUpdate(GameScene *self, float dt) {
  (void)dt;
  FailData *d = (FailData *)self->data;

  // 键盘导航：Z 确认当前选中项。失败界面无上级菜单，X 不做返回操作。
  const int prevSelected = d->nav.selected;
  MenuAction act = MenuNavUpdate(&d->nav);
  // 选中项切换（键盘 W/S/↑↓）时播放 UI 选中音效
  if (d->nav.selected != prevSelected) {
    PlaySound(d->app->uiSound);
  }
  if (act == MENU_ACTION_CONFIRM) {
    d->action = kFailActions[d->nav.selected];
  }

  // 消费动作（raygui 交互在 Draw 阶段写入，此处统一执行切换/退出）
  switch (d->action) {
  case FAIL_ACTION_QUIT_TO_MENU:
    // 清空场景栈到只剩开始菜单（回根操作）
    GameStackClearTo(self->owner, StartSceneCreate(d->app));
    break;
  case FAIL_ACTION_QUIT:
    GameStackRequestQuit(self->owner);
    break;
  default:
    break;
  }
  d->action = FAIL_ACTION_NONE;
}

static void FailDraw(GameScene *self) {
  FailData *d = (FailData *)self->data;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 全屏不透明深色底：失败画面为全屏场景（替换了关卡），不依赖下层绘制
  DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.92f));

  // 标题
  const char *title = "GAME OVER";
  const int titleSize = 44;
  DrawText(title, (screenW - MeasureText(title, titleSize)) / 2,
           screenH / 4 - titleSize / 2, titleSize, RED);

  // 副标题
  const char *subtitle = "Your HP reached 0!";
  const int subSize = 18;
  DrawText(subtitle, (screenW - MeasureText(subtitle, subSize)) / 2,
           screenH / 4 + titleSize / 2 + 12, subSize, LIGHTGRAY);

  // 按钮垂直排列
  const float btnW = 200;
  const float btnH = 44;
  const float btnX = (screenW - btnW) / 2;
  const float btnY = screenH / 2.f + 20;
  const float gap = 14;

  // 底部键盘操作提示（失败界面无返回项，故只提示移动与确认）
  const char *hint = "Move: W/S or Arrows    Confirm: Z";
  DrawText(hint, (screenW - MeasureText(hint, 14)) / 2, screenH - 24, 14,
           LIGHTGRAY);

  for (int i = 0; i < FAIL_ITEM_COUNT; i++) {
    Rectangle rec = {
        .x = btnX, .y = btnY + i * (btnH + gap), .width = btnW, .height = btnH};

    // 鼠标悬停时同步选中高亮，键盘与鼠标保持一致的选中指示
    if (CheckCollisionPointRec(GetMousePosition(), rec)) {
      d->nav.selected = i;
    }
    // 键盘选中的项以 FOCUSED 状态绘制（高亮）
    if (i == d->nav.selected) {
      GuiSetState(STATE_FOCUSED);
    }
    bool clicked = GuiButton(rec, kFailLabels[i]);
    if (i == d->nav.selected) {
      GuiSetState(STATE_NORMAL);
    }
    if (clicked) {
      d->action = kFailActions[i];
    }
  }
}

GameScene *FailSceneCreate(const GameApp *app) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  FailData *data = (FailData *)calloc(1, sizeof(FailData));
  data->app = (GameApp *)app; // 回到菜单时需把 app 传给 StartSceneCreate

  scene->name = "FailScene";
  scene->data = data;
  scene->flags = GAME_SCENE_NONE;
  scene->pauseable = false; // 失败画面不允许调出暂停界面
  scene->onEnter = FailEnter;
  scene->onUpdate = FailUpdate;
  scene->onDraw = FailDraw;
  // onExit / onPause / onResume 暂不需要，保持 NULL
  return scene;
}
