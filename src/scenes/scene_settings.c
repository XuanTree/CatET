#include "game.h"
#include <raylib.h>

// ── 设置菜单条目 ─────────────────────────────────────────────────────
// 顺序与 MenuNav.selected 索引一一对应：
//   - Sound Effect / Music 为开关（Z / 点击即时切换并持久化）；
//   - Controls / Difficulty 为说明页（Z / 点击进入，X 返回设置列表）。
// 返回只走 X / ESC 键（与其它菜单一致），不再单设 Back 按钮。
typedef enum SettingItem {
  SETTING_ITEM_SOUND = 0, // 音效总开关（Sound Effect）
  SETTING_ITEM_MUSIC,     // 音乐总开关（Music，立即停止/恢复当前 BGM 并持久化）
  SETTING_ITEM_CONTROLS,  // 操作说明（Controls，进入只读说明页）
  SETTING_ITEM_DIFFICULTY, // 难度说明（Difficulty，进入只读说明页）
  SETTING_ITEM_COUNT,
} SettingItem;

// 当前显示的设置界面：列表 / 操作说明 / 难度说明
typedef enum SettingsScreen {
  SETTINGS_SCREEN_LIST = 0, // 设置列表（Sound / Music / Controls / Difficulty）
  SETTINGS_SCREEN_CONTROLS, // 操作说明页（X / ESC 返回列表）
  SETTINGS_SCREEN_DIFFICULTY, // 难度说明页（X / ESC 返回列表）
} SettingsScreen;

// 各条目前置图标（raygui 内置图标 id，见 extern/raygui.h 的 GuiIconName）：
//   音效 → 扬声器、音乐 → 声波、操作说明 → 手动控制、难度说明 → 靶心
static const int kSettingIcons[SETTING_ITEM_COUNT] = {
    ICON_AUDIO, ICON_WAVE, ICON_MANUAL_CONTROL, ICON_TARGET};

// 说明页文本颜色（浅色卡片上的深色文字，slate-700）
static const Color kInfoTextColor = {51, 65, 85, 255};

// ── 说明页文本（内容取自 README 的 Controls / Difficulty 章节）──────────
// 仅使用 ASCII 字符：内置像素字体的字形集为 ASCII + 词库中文，方向键符号
// （←→↑↓）不在字形集内，故统一写成 Left / Right / Up / Down 以免显示为空白。
static const char *const kControlsLines[] = {
    "Move left / right     : A / D  or  Left / Right",
    "Run (move faster)     : hold Left Shift",
    "Jump (hold = higher)  : W / Up  or  Space",
    "",
    "Menu: move cursor     : W / S  or  Up / Down",
    "Menu: confirm         : Z",
    "Menu: back            : X",
    "Pause / resume        : Esc",
    "Toggle fullscreen     : F11  or  Alt + Enter",
};
#define CONTROLS_LINE_COUNT                                                    \
  ((int)(sizeof(kControlsLines) / sizeof(kControlsLines[0])))

static const char *const kDifficultyLines[] = {
    "Easy",
    "    Word pool : 100% CET-4",
    "    Max HP : 100        Battle penalty : 20",
    "    Bullets : 4-5 per wave",
    "",
    "Normal",
    "    Word pool : ~60% CET-4 + ~40% CET-6",
    "    Max HP : 100        Battle penalty : 25",
    "    Bullets : 6-7 per wave",
    "",
    "Hard",
    "    Word pool : 100% CET-6",
    "    Max HP : 125 (+25%)   Battle penalty : 30",
    "    Bullets : 8-9 per wave",
    "",
    "Bullets deal 3-8 damage on every difficulty.",
    "Time-limit / fall penalties are 15 HP.",
};
#define DIFFICULTY_LINE_COUNT                                                  \
  ((int)(sizeof(kDifficultyLines) / sizeof(kDifficultyLines[0])))

// 场景私有数据：栈持有并负责释放
typedef struct SettingsData {
  const GameApp *app;    // 只读引用，不拥有
  MenuNav nav;           // 键盘导航（W/S/↑↓ 移动，Z 确认，X 返回）
  SettingsScreen screen; // 当前所在界面（设置列表 / 说明页）
} SettingsData;

static void SettingsEnter(GameScene *self) {
  SettingsData *d = (SettingsData *)self->data;
  MenuNavInit(&d->nav, SETTING_ITEM_COUNT);
  d->screen = SETTINGS_SCREEN_LIST; // 进入时总是先显示设置列表
}

// 执行条目动作（主菜单列表 Z 确认时调用，UI 不接受鼠标输入）。
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
    // GameAppSetMusicEnabled 内部即时停止/恢复当前 BGM（曲目记忆保留，
    // 重新开启后自动续播），因此设置界面无需再操作音乐资源。
    GameAppSetMusicEnabled(app, next);
    SaveDataSaveSettings(GameAppIsSoundEnabled(d->app), next);
    break;
  }
  case SETTING_ITEM_CONTROLS:
    // 进入操作说明页（只读；X / ESC 返回设置列表，无需返回按钮）
    d->screen = SETTINGS_SCREEN_CONTROLS;
    break;
  case SETTING_ITEM_DIFFICULTY:
    // 进入难度说明页（只读；X / ESC 返回设置列表，无需返回按钮）
    d->screen = SETTINGS_SCREEN_DIFFICULTY;
    break;
  default:
    break;
  }
}

static void SettingsUpdate(GameScene *self, float dt) {
  (void)dt;
  SettingsData *d = (SettingsData *)self->data;

  // ── 说明页（Controls / Difficulty）：X / ESC 返回设置列表 ───────────
  // 说明页为只读，不设返回按钮：与其它菜单一致，X 即返回；
  // ESC 同步支持（菜单类场景 pauseable=false，ESC 由场景自行处理）。
  if (d->screen != SETTINGS_SCREEN_LIST) {
    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE)) {
      GameAppPlaySound(d->app, d->app->uiSound, d->app->uiSoundValid);
      d->screen = SETTINGS_SCREEN_LIST; // 保留原选中项，便于再次查看
    }
    return;
  }

  // ESC：返回上级菜单（弹出本覆盖层）。为什么在这里检测：主循环的 ESC
  // 状态机只响应 pauseable 场景（关卡）与暂停覆盖层，菜单类场景
  // （pauseable=false）的 ESC 由场景自己处理，与本场景的 X 返回语义一致。
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
}

// 设置条目绘制（带前置图标 + 键盘选中高亮）。
// 鼠标交互已在 UiThemeApply 中经 GuiLock 全局禁用：本函数只绘制、不处理
// 任何输入（控件返回值恒为 false，且不会再回写 active）——开关翻转与进入
// 说明页统一由键盘确认（Z）经 SettingsApplyItem 完成，开关的单一数据源
// 仍是 GameApp（见 settings 开关持久化）。
//   - 开关条目（Sound / Music）用 GuiToggle 绘制，打开态以绿色表达；
//   - 说明入口（Controls / Difficulty）用普通按钮绘制。
// 开关状态与文本由调用方逐条即时构造（含 ON/OFF）。
static void DrawSettingItem(SettingsData *d, int index, const char *label,
                            bool isToggle, bool toggleOn, float btnX,
                            float btnY, float btnW, float btnH, float gap) {
  Rectangle rec = {.x = btnX,
                   .y = btnY + index * (btnH + gap),
                   .width = btnW,
                   .height = btnH};

  const bool focused = (index == d->nav.selected);
  const int icon = kSettingIcons[index];

  if (isToggle) {
    // active 传入当前真实状态（GuiLock 后控件不再回写），仅用于绘制
    bool active = toggleOn;
    (void)UiThemeToggle(rec, icon, label, &active, focused);
  } else {
    (void)UiThemeButton(rec, icon, label, focused);
  }
}

static void SettingsDraw(GameScene *self) {
  SettingsData *d = (SettingsData *)self->data;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 半透明遮罩盖在下层菜单之上（下层 StartScene 已标记 DRAW_WHEN_HIDDEN）
  DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.55f));

  // ── 说明页（Controls / Difficulty）：标题 + 卡片，X / ESC 返回 ──────
  if (d->screen != SETTINGS_SCREEN_LIST) {
    const bool isControls = (d->screen == SETTINGS_SCREEN_CONTROLS);
    const char *title = isControls ? "Controls" : "Difficulty";
    const int titleSize = 40;
    GameAppDrawText(d->app, title,
                    (screenW - GameAppMeasureText(d->app, title, titleSize)) /
                        2,
                    screenH / 8 - titleSize / 2, titleSize, WHITE);

    // 逐行绘制说明文本（左对齐；空串用作段间空隙），放在浅色卡片上
    const char *const *lines = isControls ? kControlsLines : kDifficultyLines;
    const int lineCount =
        isControls ? CONTROLS_LINE_COUNT : DIFFICULTY_LINE_COUNT;
    const int font = 16;
    const int lineH = 18; // 17 行说明 + 内边距仍可完整容纳于 480 高度内
    const int padX = 16;
    const int padY = 14;

    // 卡片宽度随文本自适应并钳制在屏幕内
    int textW = 0;
    for (int i = 0; i < lineCount; i++) {
      const int w = GameAppMeasureText(d->app, lines[i], font);
      if (w > textW)
        textW = w;
    }
    float panelW = (float)textW + (float)(padX * 2);
    const float maxPanelW = (float)screenW - 24.0f;
    if (panelW > maxPanelW)
      panelW = maxPanelW;
    const float panelH = (float)(lineCount * lineH) + (float)(padY * 2);
    const float panelX = ((float)screenW - panelW) * 0.5f;
    const float panelY = screenH / 8.f + titleSize / 2.f + 14.0f;
    UiThemePanel((Rectangle){panelX, panelY, panelW, panelH});

    for (int i = 0; i < lineCount; i++) {
      GameAppDrawText(d->app, lines[i], (int)panelX + padX,
                      (int)panelY + padY + i * lineH, font, kInfoTextColor);
    }

    // 底部提示：X / ESC 返回设置列表（说明页不设返回按钮）
    const char *hint = "Back: X / ESC";
    GameAppDrawText(d->app, hint,
                    (screenW - GameAppMeasureText(d->app, hint, 16)) / 2,
                    screenH - 24, 16, LIGHTGRAY);
    return;
  }

  // ── 设置列表：卡片 + 四个条目（Sound / Music / Controls / Difficulty）──
  // 标题（使用全局像素字体）
  const char *title = "Settings";
  const int titleSize = 40;
  GameAppDrawText(d->app, title,
                  (screenW - GameAppMeasureText(d->app, title, titleSize)) / 2,
                  screenH / 8 - titleSize / 2, titleSize, WHITE);

  // 卡片面板承载按钮（浅色底，与深色遮罩形成层次）
  const float btnW = 240;
  const float btnH = 44;
  const float gap = 14;
  const float panelW = btnW + 36;
  const float panelH =
      SETTING_ITEM_COUNT * btnH + (SETTING_ITEM_COUNT - 1) * gap + 24;
  const float panelX = (screenW - panelW) / 2;
  const float panelY = screenH / 8.f + titleSize / 2.f + 14.0f;
  UiThemePanel((Rectangle){panelX, panelY, panelW, panelH});

  const float btnX = panelX + (panelW - btnW) / 2;
  const float btnY = panelY + 12;

  // 逐条构造文本并立即绘制：TextFormat 字符串只在本次迭代内使用，
  // 避免其轮换静态缓冲区被后续条目覆盖
  for (int i = 0; i < SETTING_ITEM_COUNT; i++) {
    const char *label;
    bool isToggle = false;
    bool toggleOn = false;
    switch (i) {
    case SETTING_ITEM_SOUND:
      isToggle = true;
      toggleOn = GameAppIsSoundEnabled(d->app);
      label = TextFormat("Sound Effect: %s", toggleOn ? "ON" : "OFF");
      break;
    case SETTING_ITEM_MUSIC:
      isToggle = true;
      toggleOn = GameAppIsMusicEnabled(d->app);
      label = TextFormat("Music: %s", toggleOn ? "ON" : "OFF");
      break;
    case SETTING_ITEM_CONTROLS:
      label = "Controls";
      break;
    default:
      label = "Difficulty";
      break;
    }
    DrawSettingItem(d, i, label, isToggle, toggleOn, btnX, btnY, btnW, btnH,
                    gap);
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
