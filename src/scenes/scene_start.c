#include "scenes/scene_start.h"
#include "scenes/scene_transition.h"
#include "systems/level_flow.h"
#include "tools/menu.h"
#include "tools/raygui.h"
#include <raylib.h>

// ── 开始菜单交互结果 ───────────────────────────────────────────────
typedef enum StartAction {
  START_ACTION_NONE = 0,
  START_ACTION_PLAY,     // 开始游戏 → 进入难度选择二级菜单
  START_ACTION_SETTINGS, // 设置（暂未实现，占位）
  START_ACTION_QUIT,     // 退出游戏
} StartAction;

// 开始菜单选项定义（顺序与 MenuNav.selected 索引一一对应）
static const char *const kStartLabels[] = {"Play", "Settings", "Quit"};
static const StartAction kStartActions[] = {
    START_ACTION_PLAY, START_ACTION_SETTINGS, START_ACTION_QUIT};
#define START_ITEM_COUNT 3

// ── 难度选择二级菜单 ─────────────────────────────────────────────────
// 选中 Play 后弹出：玩家可在 Easy / Normal / Hard 中选择初始难度
// （0=简单 CET4、1=普通 CET4、2=困难 CET6，与 scene_test/scene_maze 一致）。
// 难度一经选择即随 LevelFlow 传递到后续所有关卡；按 X 键返回主菜单。
static const char *const kDiffLabels[] = {"Easy", "Normal", "Hard"};
#define DIFF_ITEM_COUNT 3

// 当前显示的菜单界面
typedef enum StartScreen {
  START_SCREEN_MAIN = 0,   // 主菜单
  START_SCREEN_DIFFICULTY, // 难度选择二级菜单
} StartScreen;

// 场景私有数据：栈持有并负责释放
typedef struct StartData {
  const GameApp *app; // 只读引用，不拥有
  MenuNav nav;        // 主菜单键盘导航（W/S/↑↓ 移动，Z 确认，X 返回）
  MenuNav diffNav;    // 难度菜单键盘导航
  StartScreen screen; // 当前所在菜单界面
  StartAction action; // 主菜单动作：Draw/Update 阶段由按钮产生，Update 消费执行
  int diffAction;     // 难度菜单动作：-1=无、0/1/2=按该难度开始游戏
} StartData;

static void StartEnter(GameScene *self) {
  StartData *d = (StartData *)self->data;
  // 进入开始菜单即开启新的一局：重置跨关卡生命值继承（下一关从满血开始）
  ((GameApp *)d->app)->playerHealth = 0.0f;
  d->screen = START_SCREEN_MAIN;
  d->diffAction = -1;
  MenuNavInit(&d->nav, START_ITEM_COUNT);
  MenuNavInit(&d->diffNav, DIFF_ITEM_COUNT);
}

static void StartUpdate(GameScene *self, float dt) {
  (void)dt;
  StartData *d = (StartData *)self->data;

  // ── 难度选择二级菜单 ─────────────────────────────────────────────
  if (d->screen == START_SCREEN_DIFFICULTY) {
    const int prevSelected = d->diffNav.selected;
    MenuAction act = MenuNavUpdate(&d->diffNav);
    // 选中项切换（W/S/↑↓）或确认/返回（Z/X）时播放 UI 音效
    if (d->diffNav.selected != prevSelected || act == MENU_ACTION_CONFIRM ||
        act == MENU_ACTION_BACK) {
      if (d->app->uiSoundValid)
        PlaySound(d->app->uiSound);
    }
    if (act == MENU_ACTION_CONFIRM) {
      d->diffAction = d->diffNav.selected; // 0/1/2 对应 Easy/Normal/Hard
    } else if (act == MENU_ACTION_BACK) {
      // X：返回主菜单（无需返回按钮）
      d->screen = START_SCREEN_MAIN;
      MenuNavInit(&d->diffNav, DIFF_ITEM_COUNT);
      d->diffAction = -1;
      return;
    }

    // 消费难度动作（键盘导航或鼠标点击写入），统一在此执行
    if (d->diffAction >= 0) {
      // 按所选难度开始游戏（平台跳跃第 1 关），难度随 LevelFlow 传递
      GameStackReplace(self->owner, TransitionSceneCreate(
                                        d->app, LevelFlowCreateScene(
                                                    d->app, LEVEL_TYPE_PLATFORM,
                                                    1, d->diffAction)));
      return;
    }
    return;
  }

  // ── 主菜单 ──────────────────────────────────────────────────────
  const int prevSelected = d->nav.selected;
  MenuAction act = MenuNavUpdate(&d->nav);
  // 选中项切换（键盘 W/S/↑↓）或确认（Z）时播放 UI 音效
  if (d->nav.selected != prevSelected || act == MENU_ACTION_CONFIRM) {
    if (d->app->uiSoundValid)
      PlaySound(d->app->uiSound);
  }
  if (act == MENU_ACTION_CONFIRM) {
    d->action = kStartActions[d->nav.selected];
  }

  // 消费动作（raygui 交互在 Draw 阶段写入，此处统一执行切换/退出）
  switch (d->action) {
  case START_ACTION_PLAY:
    // 进入难度选择二级菜单，由玩家指定初始难度后再开赛
    d->screen = START_SCREEN_DIFFICULTY;
    MenuNavInit(&d->diffNav, DIFF_ITEM_COUNT);
    d->diffAction = -1;
    break;
  case START_ACTION_SETTINGS:
    // 设置界面暂未实现，仅占位
    break;
  case START_ACTION_QUIT:
    GameStackRequestQuit(self->owner);
    break;
  default:
    break;
  }
  d->action = START_ACTION_NONE;
}

// 主菜单按钮绘制（Play / Settings / Quit）
static void DrawMainMenu(StartData *d) {
  const float btnW = 180;
  const float btnH = 44;
  const float btnX = (d->app->logicWidth - btnW) / 2;
  const float btnY = d->app->logicHeight / 2.f + 20;
  const float gap = 14;

  for (int i = 0; i < START_ITEM_COUNT; i++) {
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
    bool clicked = GuiButton(rec, kStartLabels[i]);
    if (i == d->nav.selected) {
      GuiSetState(STATE_NORMAL);
    }
    if (clicked) {
      d->action = kStartActions[i];
    }
  }
}

// 难度选择菜单按钮绘制（Easy / Normal / Hard；X 键返回主菜单）
static void DrawDifficultyMenu(StartData *d) {
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 标题置于副标题之下、按钮之上，避免与标题/副标题重叠
  const char *diffTitle = "Select Difficulty";
  const int dtSize = 22;
  const int titleSize = 48; // 与 StartDraw 中主标题字号一致
  const int subSize = 20;   // 与 StartDraw 中副标题字号一致
  const int dtY = screenH / 4 + titleSize / 2 + 12 + subSize + 20;
  GameAppDrawText(d->app, diffTitle,
                  (screenW - GameAppMeasureText(d->app, diffTitle, dtSize)) / 2,
                  dtY, dtSize, DARKGRAY);

  const float btnW = 200;
  const float btnH = 44;
  const float btnX = (screenW - btnW) / 2;
  const float btnY = screenH / 2.f + 20;
  const float gap = 14;

  for (int i = 0; i < DIFF_ITEM_COUNT; i++) {
    Rectangle rec = {
        .x = btnX, .y = btnY + i * (btnH + gap), .width = btnW, .height = btnH};

    // 鼠标悬停时同步选中高亮，键盘与鼠标保持一致的选中指示
    if (CheckCollisionPointRec(GetMousePosition(), rec)) {
      d->diffNav.selected = i;
    }
    if (i == d->diffNav.selected) {
      GuiSetState(STATE_FOCUSED);
    }
    bool clicked = GuiButton(rec, kDiffLabels[i]);
    if (i == d->diffNav.selected) {
      GuiSetState(STATE_NORMAL);
    }
    if (clicked) {
      d->diffAction = i;
    }
  }
}

static void StartDraw(GameScene *self) {
  StartData *d = (StartData *)self->data;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 标题（使用全局像素字体，与整体 UI 风格一致）
  const char *title = "CatET";
  const int titleSize = 48;
  GameAppDrawText(d->app, title,
                  (screenW - GameAppMeasureText(d->app, title, titleSize)) / 2,
                  screenH / 4 - titleSize / 2, titleSize, DARKGRAY);

  const char *subtitle = "CET Words Challenge";
  const int subSize = 20;
  GameAppDrawText(d->app, subtitle,
                  (screenW - GameAppMeasureText(d->app, subtitle, subSize)) / 2,
                  screenH / 4 + titleSize / 2 + 12, subSize, GRAY);

  // 按当前所在菜单绘制对应按钮组
  if (d->screen == START_SCREEN_DIFFICULTY) {
    DrawDifficultyMenu(d);
  } else {
    DrawMainMenu(d);
  }

  // 底部键盘操作提示
  // 字号取 16 = UI_FONT_BASE_SIZE(48)/3 的整数倍，避免像素字点采样在
  // 非整数倍（14px）缩放下字形下半部分像素丢失导致提示显示不全。
  const char *hint = "Move: W/S or Arrows    Confirm: Z    Back: X";
  GameAppDrawText(d->app, hint,
                  (screenW - GameAppMeasureText(d->app, hint, 16)) / 2,
                  screenH - 24, 16, GRAY);
}

static void StartExit(GameScene *self) {
  (void)self;
  // 菜单未加载资源，无需释放
}

GameScene *StartSceneCreate(GameApp *app) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  StartData *data = (StartData *)calloc(1, sizeof(StartData));
  data->app = app;
  data->diffAction = -1;

  scene->name = "StartScene";
  scene->data = data;
  scene->flags = GAME_SCENE_NONE;
  scene->pauseable = false; // 开始界面不允许调出暂停画面
  scene->onEnter = StartEnter;
  scene->onUpdate = StartUpdate;
  scene->onDraw = StartDraw;
  scene->onExit = StartExit;
  // onPause / onResume 暂不需要，保持 NULL
  return scene;
}
