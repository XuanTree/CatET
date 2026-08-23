#include "scenes/scene_start.h"
#include "scenes/scene_test.h"
#include "scenes/scene_transition.h"
#include "tools/menu.h"
#include "tools/raygui.h"
#include <raylib.h>

// ── 开始菜单交互结果 ───────────────────────────────────────────────
typedef enum StartAction {
  START_ACTION_NONE = 0,
  START_ACTION_PLAY,     // 开始游戏 → 进入测试关卡
  START_ACTION_SETTINGS, // 设置（暂未实现，占位）
  START_ACTION_QUIT,     // 退出游戏
} StartAction;

// 开始菜单选项定义（顺序与 MenuNav.selected 索引一一对应）
static const char *const kStartLabels[] = {"Play", "Settings", "Quit"};
static const StartAction kStartActions[] = {
    START_ACTION_PLAY, START_ACTION_SETTINGS, START_ACTION_QUIT};
#define START_ITEM_COUNT 3

// 场景私有数据：栈持有并负责释放
typedef struct StartData {
  const GameApp *app; // 只读引用，不拥有
  MenuNav nav;        // 键盘导航（W/S/↑↓ 移动，Z 确认，X 返回）
  StartAction action; // Draw/Update 阶段由按钮产生，Update 消费执行
} StartData;

static void StartEnter(GameScene *self) {
  StartData *d = (StartData *)self->data;
  MenuNavInit(&d->nav, START_ITEM_COUNT);
}

static void StartUpdate(GameScene *self, float dt) {
  (void)dt;
  StartData *d = (StartData *)self->data;

  // 键盘导航：Z 确认当前选中项；X 返回上级菜单（开始界面为顶级菜单，无上级）
  const int prevSelected = d->nav.selected;
  if (MenuNavUpdate(&d->nav) == MENU_ACTION_CONFIRM) {
    d->action = kStartActions[d->nav.selected];
  }
  // 选中项切换（键盘 W/S/↑↓）时播放 UI 选中音效
  if (d->nav.selected != prevSelected) {
    PlaySound(d->app->uiSound);
  }

  // 消费动作（raygui 交互在 Draw 阶段写入，此处统一执行切换/退出）
  switch (d->action) {
  case START_ACTION_PLAY:
    // 经过渡场景进入测试关卡：先替换为过渡场景（淡入淡出遮罩），
    // 过渡结束后再自动替换为目标关卡，见 scene_transition.h。
    GameStackReplace(self->owner,
                     TransitionSceneCreate(d->app, TestSceneCreate(d->app)));
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

static void StartDraw(GameScene *self) {
  StartData *d = (StartData *)self->data;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 标题
  const char *title = "CatET";
  const int titleSize = 48;
  DrawText(title, (screenW - MeasureText(title, titleSize)) / 2,
           screenH / 4 - titleSize / 2, titleSize, DARKGRAY);

  const char *subtitle = "CET Words Challenge";
  const int subSize = 20;
  DrawText(subtitle, (screenW - MeasureText(subtitle, subSize)) / 2,
           screenH / 4 + titleSize / 2 + 12, subSize, GRAY);

  // 按钮垂直排列
  const float btnW = 180;
  const float btnH = 44;
  const float btnX = (screenW - btnW) / 2;
  const float btnY = screenH / 2.f + 20;
  const float gap = 14;

  // 底部键盘操作提示
  const char *hint = "Move: W/S or Arrows    Confirm: Z    Back: X";
  DrawText(hint, (screenW - MeasureText(hint, 14)) / 2, screenH - 24, 14, GRAY);

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

static void StartExit(GameScene *self) {
  (void)self;
  // 菜单未加载资源，无需释放
}

GameScene *StartSceneCreate(GameApp *app) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  StartData *data = (StartData *)calloc(1, sizeof(StartData));
  data->app = app;

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
