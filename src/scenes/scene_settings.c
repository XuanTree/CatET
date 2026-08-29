#include "game.h"
#include <raylib.h>

// ── 设置菜单条目 ─────────────────────────────────────────────────────
// 顺序与 MenuNav.selected 索引一一对应；均为开关（Z/点击切换）。
// 返回只走 X / ESC 键（与其它菜单一致），不再单设 Back 按钮。
typedef enum SettingItem {
  SETTING_ITEM_SOUND = 0, // 音效总开关（Sound Effect）
  SETTING_ITEM_MUSIC,     // 音乐总开关（Music，当前无音乐资源，接口预留）
  SETTING_ITEM_COUNT,
} SettingItem;

// 场景私有数据：栈持有并负责释放
typedef struct SettingsData {
  const GameApp *app; // 只读引用，不拥有
  MenuNav nav;        // 键盘导航（W/S/↑↓ 移动，Z 确认，X 返回）
  int action;         // 鼠标点击的条目索引（Draw 阶段写入，Update 消费）；
                      // -1 表示无动作，与难度菜单的 diffAction 约定一致
} SettingsData;

static void SettingsEnter(GameScene *self) {
  SettingsData *d = (SettingsData *)self->data;
  MenuNavInit(&d->nav, SETTING_ITEM_COUNT);
  d->action = -1;
}

// 执行条目动作（Z 确认 / 鼠标点击共用）。
// 开关值直接读写 GameApp 字段并立即持久化，无需在场景内缓存：
// 任何入口改动都即时生效，返回上级后再进也显示最新状态。
static void SettingsApplyItem(GameScene *self, SettingItem item) {
  SettingsData *d = (SettingsData *)self->data;
  GameApp *app = (GameApp *)d->app; // 切换开关需改写 app

  switch (item) {
  case SETTING_ITEM_SOUND: {
    const bool next = !GameAppIsSoundEnabled(d->app);
    GameAppSetSoundEnabled(app, next);
    SaveDataSaveSettings(next, GameAppIsMusicEnabled(d->app));
    // 重新开启时补播一次确认音，让玩家立刻听到音效已恢复
    // （关闭方向的确认音已在切换前播放，可被听到）
    if (next) {
      GameAppPlaySound(d->app, d->app->uiSound, d->app->uiSoundValid);
    }
    break;
  }
  case SETTING_ITEM_MUSIC: {
    const bool next = !GameAppIsMusicEnabled(d->app);
    GameAppSetMusicEnabled(app, next);
    SaveDataSaveSettings(GameAppIsSoundEnabled(d->app), next);
    break;
  }
  default:
    break;
  }
}

static void SettingsUpdate(GameScene *self, float dt) {
  (void)dt;
  SettingsData *d = (SettingsData *)self->data;

  // ESC：返回上级菜单。为什么在这里检测：主循环的 ESC 状态机只响应
  // pauseable 场景（关卡）与暂停覆盖层，菜单类场景（pauseable=false）
  // 的 ESC 由场景自己处理，与本场景的 X 返回语义一致。
  if (IsKeyPressed(KEY_ESCAPE)) {
    GameStackPop(self->owner);
    return;
  }

  const int prevSelected = d->nav.selected;
  MenuAction act = MenuNavUpdate(&d->nav);
  // 选中项切换（W/S/↑↓）、确认（Z）或返回（X）时播放 UI 音效
  if (d->nav.selected != prevSelected || act != MENU_ACTION_NONE) {
    GameAppPlaySound(d->app, d->app->uiSound, d->app->uiSoundValid);
  }
  if (act == MENU_ACTION_BACK) {
    GameStackPop(self->owner);
    return;
  }
  if (act == MENU_ACTION_CONFIRM) {
    SettingsApplyItem(self, (SettingItem)d->nav.selected);
    return;
  }

  // 消费鼠标点击（Draw 阶段写入），统一执行
  if (d->action >= 0) {
    SettingsApplyItem(self, (SettingItem)d->action);
    d->action = -1;
  }
}

// 设置开关按钮绘制（开关状态拼进按钮文本，如 "Sound Effect: ON"）
static void DrawSettingButton(SettingsData *d, int index, const char *label,
                              float btnX, float btnY, float btnW, float btnH,
                              float gap) {
  Rectangle rec = {.x = btnX,
                   .y = btnY + index * (btnH + gap),
                   .width = btnW,
                   .height = btnH};

  // 鼠标悬停时同步选中高亮，键盘与鼠标保持一致的选中指示
  if (CheckCollisionPointRec(GetMousePosition(), rec)) {
    d->nav.selected = index;
  }
  if (index == d->nav.selected) {
    GuiSetState(STATE_FOCUSED);
  }
  bool clicked = GuiButton(rec, label);
  if (index == d->nav.selected) {
    GuiSetState(STATE_NORMAL);
  }
  if (clicked) {
    d->action = index;
  }
}

static void SettingsDraw(GameScene *self) {
  SettingsData *d = (SettingsData *)self->data;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 半透明遮罩盖在下层菜单之上（下层 StartScene 已标记 DRAW_WHEN_HIDDEN）
  DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.55f));

  // 标题（使用全局像素字体）
  const char *title = "Settings";
  const int titleSize = 40;
  GameAppDrawText(d->app, title,
                  (screenW - GameAppMeasureText(d->app, title, titleSize)) / 2,
                  screenH / 4 - titleSize / 2, titleSize, WHITE);

  // 两个开关条目垂直排列：音效开关 / 音乐开关
  const float btnW = 240;
  const float btnH = 44;
  const float btnX = (screenW - btnW) / 2;
  const float btnY = screenH / 2.f;
  const float gap = 14;

  // TextFormat 每次调用覆盖同一静态缓冲区，逐次使用并立即绘制
  const char *soundLabel =
      TextFormat("Sound Effect: %s",
                 GameAppIsSoundEnabled(d->app) ? "ON" : "OFF");
  const char *musicLabel =
      TextFormat("Music: %s", GameAppIsMusicEnabled(d->app) ? "ON" : "OFF");
  const char *labels[SETTING_ITEM_COUNT] = {soundLabel, musicLabel};

  for (int i = 0; i < SETTING_ITEM_COUNT; i++) {
    DrawSettingButton(d, i, labels[i], btnX, btnY, btnW, btnH, gap);
  }

  // 底部键盘操作提示（字号取 16 = UI_FONT_BASE_SIZE(48)/3 的整数倍，
  // 避免像素字点采样在非整数倍缩放下字形下半部分像素丢失）
  const char *hint = "Move: W/S or Arrows    Confirm: Z    Back: X / ESC";
  GameAppDrawText(d->app, hint,
                  (screenW - GameAppMeasureText(d->app, hint, 16)) / 2,
                  screenH - 24, 16, LIGHTGRAY);
}

GameScene *SettingsSceneCreate(const GameApp *app) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  SettingsData *data = (SettingsData *)calloc(1, sizeof(SettingsData));
  data->app = app;
  data->action = -1;

  scene->name = "SettingsScene";
  scene->data = data;
  scene->flags = GAME_SCENE_NONE;
  scene->pauseable = false; // 设置界面不允许调出暂停画面
  scene->onEnter = SettingsEnter;
  scene->onUpdate = SettingsUpdate;
  scene->onDraw = SettingsDraw;
  // onExit / onPause / onResume 暂不需要，保持 NULL
  return scene;
}
