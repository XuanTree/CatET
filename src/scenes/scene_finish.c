#include "game.h"
#include <raylib.h>
#include <stdlib.h>

// ── 通关结算界面交互结果 ───────────────────────────────────────────────
typedef enum FinishAction {
  FINISH_ACTION_NONE = 0,
  FINISH_ACTION_BACK_TO_MENU, // 回到菜单（清空栈到开始界面）
} FinishAction;

// 通关结算界面选项定义（顺序与 MenuNav.selected 索引一一对应）。
// 玩家通关后只能选择「回到菜单」一项。
static const char *const kFinishLabels[] = {"Back to Menu"};
static const FinishAction kFinishActions[] = {FINISH_ACTION_BACK_TO_MENU};
#define FINISH_ITEM_COUNT 1

// 场景私有数据：栈持有并负责释放
typedef struct FinishData {
  GameApp *app;        // 引用（不拥有）；回到菜单时需传给 StartSceneCreate
  MenuNav nav;         // 键盘导航（W/S/↑↓ 移动，Z 确认）
  FinishAction action; // Draw/Update 阶段由按钮产生，Update 消费执行
} FinishData;

static void FinishEnter(GameScene *self) {
  FinishData *d = (FinishData *)self->data;
  // 通关胜利音效（game_finish.ogg）：进入通关结算界面时播放
  GameAppPlaySound(d->app, d->app->gameFinishSound,
                   d->app->gameFinishSoundValid);
  MenuNavInit(&d->nav, FINISH_ITEM_COUNT);
}

static void FinishUpdate(GameScene *self, float dt) {
  (void)dt;
  FinishData *d = (FinishData *)self->data;

  // 键盘导航：Z 确认当前选中项。通关结算界面无上级菜单，X 不做返回操作。
  const int prevSelected = d->nav.selected;
  MenuAction act = MenuNavUpdate(&d->nav);
  // 选中项切换（W/S/↑↓）或确认（Z）时播放 UI 音效
  if (d->nav.selected != prevSelected || act == MENU_ACTION_CONFIRM) {
    GameAppPlaySound(d->app, d->app->uiSound, d->app->uiSoundValid);
  }
  if (act == MENU_ACTION_CONFIRM) {
    d->action = kFinishActions[d->nav.selected];
  }

  // 消费动作（raygui 交互在 Draw 阶段写入，此处统一执行切换/退出）
  switch (d->action) {
  case FINISH_ACTION_BACK_TO_MENU:
    // 清空场景栈到只剩开始菜单（回根操作）
    GameStackClearTo(self->owner, StartSceneCreate(d->app));
    break;
  default:
    break;
  }
  d->action = FINISH_ACTION_NONE;
}

static void FinishDraw(GameScene *self) {
  FinishData *d = (FinishData *)self->data;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 全屏不透明深色底：通关结算画面为全屏场景（替换了关卡），不依赖下层绘制
  DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.92f));

  // 标题（使用全局像素字体）
  const char *title = "Congratulations!";
  const int titleSize = 40;
  GameAppDrawText(d->app, title,
                  (screenW - GameAppMeasureText(d->app, title, titleSize)) / 2,
                  screenH / 4 - titleSize / 2, titleSize, GREEN);

  // 副标题
  const char *subtitle = "You cleared all levels!";
  const int subSize = 18;
  GameAppDrawText(d->app, subtitle,
                  (screenW - GameAppMeasureText(d->app, subtitle, subSize)) / 2,
                  screenH / 4 + titleSize / 2 + 12, subSize, LIGHTGRAY);

  // 本轮通关时间：SpeedrunFinish 已停止速通计时，speedrunElapsed 冻结为
  // 本轮实际耗时；主菜单仍继续显示最佳时间（见 scene_start.c）。
  const char *timeText =
      TextFormat("Time %02d:%02d", (int)d->app->speedrunElapsed / 60,
                 (int)d->app->speedrunElapsed % 60);
  const int timeSize = 20;
  GameAppDrawText(
      d->app, timeText,
      (screenW - GameAppMeasureText(d->app, timeText, timeSize)) / 2,
      screenH / 4 + titleSize / 2 + 12 + subSize + 24, timeSize, GOLD);

  // 按钮（唯一交互项）：Back to Menu
  const float btnW = 200;
  const float btnH = 44;
  const float btnX = (screenW - btnW) / 2;
  const float btnY = screenH / 2.f + 20;
  const float gap = 14;

  // 底部键盘操作提示（通关结算界面仅一个按钮，无返回项）
  // 字号取 16 = UI_FONT_BASE_SIZE(48)/3 的整数倍，避免像素字点采样在
  // 非整数倍（14px）缩放下字形下半部分像素丢失导致提示显示不全。
  const char *hint = "Move: W/S or Arrows    Confirm: Z";
  GameAppDrawText(d->app, hint,
                  (screenW - GameAppMeasureText(d->app, hint, 16)) / 2,
                  screenH - 24, 16, LIGHTGRAY);

  for (int i = 0; i < FINISH_ITEM_COUNT; i++) {
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
    bool clicked = GuiButton(rec, kFinishLabels[i]);
    if (i == d->nav.selected) {
      GuiSetState(STATE_NORMAL);
    }
    if (clicked) {
      d->action = kFinishActions[i];
    }
  }
}

GameScene *FinishSceneCreate(const GameApp *app) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  FinishData *data = (FinishData *)calloc(1, sizeof(FinishData));
  data->app = (GameApp *)app; // 回到菜单时需把 app 传给 StartSceneCreate

  scene->name = "FinishScene";
  scene->data = data;
  scene->flags = GAME_SCENE_NONE;
  scene->pauseable = false; // 通关结算画面不允许调出暂停界面
  scene->onEnter = FinishEnter;
  scene->onUpdate = FinishUpdate;
  scene->onDraw = FinishDraw;
  // onExit / onPause / onResume 暂不需要，保持 NULL
  return scene;
}
