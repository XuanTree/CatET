#include "game.h"
#include <raylib.h>

// ── 暂停菜单交互结果 ───────────────────────────────────────────────
typedef enum PauseAction {
  PAUSE_ACTION_NONE = 0,
  PAUSE_ACTION_RESUME,       // 继续游戏
  PAUSE_ACTION_QUIT_TO_MENU, // 返回开始界面
  PAUSE_ACTION_QUIT,         // 退出游戏
} PauseAction;

// 暂停菜单选项定义（顺序与 MenuNav.selected 索引一一对应）
static const char *const kPauseLabels[] = {"Resume", "Quit to Menu", "Quit"};
static const PauseAction kPauseActions[] = {
    PAUSE_ACTION_RESUME, PAUSE_ACTION_QUIT_TO_MENU, PAUSE_ACTION_QUIT};
#define PAUSE_ITEM_COUNT 3

// 场景私有数据：栈持有并负责释放
typedef struct PauseData {
  GameApp *app;       // 引用（不拥有）；退出暂停时需复位 app->isPaused
  MenuNav nav;        // 键盘导航（W/S/↑↓ 移动，Z 确认，X 返回）
  PauseAction action; // Draw/Update 阶段由按钮产生，Update 消费执行
} PauseData;

static void PauseEnter(GameScene *self) {
  PauseData *d = (PauseData *)self->data;
  MenuNavInit(&d->nav, PAUSE_ITEM_COUNT);
}

static void PauseUpdate(GameScene *self, float dt) {
  (void)dt;
  PauseData *d = (PauseData *)self->data;

  // 键盘导航：X 返回上级（关闭暂停），Z 确认当前选中项。
  // 注意：ESC 的进入/退出统一由主循环按 app.isPaused 状态机处理，
  // 此处不再检测 ESC，避免同一 ESC 事件导致刚推入的暂停界面同帧被弹出。
  const int prevSelected = d->nav.selected;
  MenuAction act = MenuNavUpdate(&d->nav);
  // 选中项切换（W/S/↑↓）、确认（Z）或返回（X）时播放 UI 音效
  if (d->nav.selected != prevSelected || act != MENU_ACTION_NONE) {
    GameAppPlaySound(d->app, d->app->uiSound, d->app->uiSoundValid);
  }
  if (act == MENU_ACTION_BACK) {
    d->app->isPaused = false;
    GameStackPop(self->owner);
    return;
  }
  if (act == MENU_ACTION_CONFIRM) {
    d->action = kPauseActions[d->nav.selected];
  }

  // 消费动作（含鼠标点击写入的 action），统一执行并复位暂停标志
  switch (d->action) {
  case PAUSE_ACTION_RESUME:
    d->app->isPaused = false;
    GameStackPop(self->owner); // 弹回关卡
    break;
  case PAUSE_ACTION_QUIT_TO_MENU:
    d->app->isPaused = false;
    // 清空场景栈到只剩开始菜单
    GameStackClearTo(self->owner, StartSceneCreate(d->app));
    break;
  case PAUSE_ACTION_QUIT:
    d->app->isPaused = false;
    GameStackRequestQuit(self->owner);
    break;
  default:
    break;
  }
  d->action = PAUSE_ACTION_NONE;
}

static void PauseDraw(GameScene *self) {
  PauseData *d = (PauseData *)self->data;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 半透明遮罩盖在下层关卡之上（下层 TestScene 已标记 DRAW_WHEN_HIDDEN）
  DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.55f));

  // 标题（使用全局像素字体）
  const char *title = "PAUSED";
  const int titleSize = 40;
  GameAppDrawText(d->app, title,
                  (screenW - GameAppMeasureText(d->app, title, titleSize)) / 2,
                  screenH / 4 - titleSize / 2, titleSize, WHITE);

  // 按钮垂直排列
  const float btnW = 200;
  const float btnH = 44;
  const float btnX = (screenW - btnW) / 2;
  const float btnY = screenH / 2.f;
  const float gap = 14;

  // 底部键盘操作提示
  // 字号取 16 = UI_FONT_BASE_SIZE(48)/3 的整数倍，避免像素字点采样在
  // 非整数倍（14px）缩放下字形下半部分像素丢失导致提示显示不全。
  const char *hint = "Move: W/S or Arrows    Confirm: Z    Back: X / ESC";
  GameAppDrawText(d->app, hint,
                  (screenW - GameAppMeasureText(d->app, hint, 16)) / 2,
                  screenH - 24, 16, LIGHTGRAY);

  for (int i = 0; i < PAUSE_ITEM_COUNT; i++) {
    Rectangle rec = {
        .x = btnX, .y = btnY + i * (btnH + gap), .width = btnW, .height = btnH};

    // 鼠标悬停时同步选中高亮
    if (CheckCollisionPointRec(GetMousePosition(), rec)) {
      d->nav.selected = i;
    }
    // 键盘选中的项以 FOCUSED 状态绘制（高亮）
    if (i == d->nav.selected) {
      GuiSetState(STATE_FOCUSED);
    }
    bool clicked = GuiButton(rec, kPauseLabels[i]);
    if (i == d->nav.selected) {
      GuiSetState(STATE_NORMAL);
    }
    if (clicked) {
      d->action = kPauseActions[i];
    }
  }
}

GameScene *PauseSceneCreate(const GameApp *app) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  PauseData *data = (PauseData *)calloc(1, sizeof(PauseData));
  data->app = (GameApp *)app; // 退出暂停时需复位 app->isPaused

  scene->name = "PauseScene";
  scene->data = data;
  scene->flags = GAME_SCENE_NONE;
  scene->pauseable = false; // 暂停界面自身不可再被暂停（防止主循环重复弹入）
  scene->onEnter = PauseEnter;
  scene->onUpdate = PauseUpdate;
  scene->onDraw = PauseDraw;
  // onExit / onPause / onResume 暂不需要，保持 NULL
  return scene;
}
