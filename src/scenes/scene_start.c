#include "game.h"
#include <raylib.h>

// ── 开始菜单交互结果 ───────────────────────────────────────────────
typedef enum StartAction {
  START_ACTION_NONE = 0,
  START_ACTION_PLAY,     // 开始游戏 → 进入难度选择二级菜单（主线 100 关）
  START_ACTION_INFINITE, // 无尽模式 → 进入难度选择二级菜单（scene_infinite）
  START_ACTION_SETTINGS, // 设置 → 压入设置覆盖层（scene_settings）
  START_ACTION_QUIT,     // 退出游戏
} StartAction;

// 开始菜单选项定义（顺序与 MenuNav.selected 索引一一对应）
static const char *const kStartLabels[] = {"Play", "Infinite", "Settings",
                                           "Quit"};
static const StartAction kStartActions[] = {
    START_ACTION_PLAY, START_ACTION_INFINITE, START_ACTION_SETTINGS,
    START_ACTION_QUIT};
#define START_ITEM_COUNT 4

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

// ── 标题文案与「标题小猫」彩蛋参数 ────────────────────────────────────
// 主菜单与难度菜单共用同一标题（同位置、同字号），小猫彩蛋据此定位。
static const char *const kTitleText = "CatET";
#define START_TITLE_SIZE 48 // 标题字号（像素字体基准字号的整数倍，放大不糊）
// 小猫贴图 icon.png 为 16×16 像素画：按 GAME_SCALE 整数放大到 48，与标题
// 字高相当；底部上移 START_TITLE_SINK
// 像素与字形顶部相叠，视觉上「坐」在字母上。
#define START_TITLE_SINK 6.0f
// 轻微起伏（幅度 px、频率 Hz）：静态标题上多一点生气，不影响任何交互
#define START_TITLE_BOB_PX 2.0f
#define START_TITLE_BOB_HZ 2.0f

// 场景私有数据：栈持有并负责释放
typedef struct StartData {
  const GameApp *app; // 只读引用，不拥有
  MenuNav nav;        // 主菜单键盘导航（W/S/↑↓ 移动，Z 确认，X 返回）
  MenuNav diffNav;    // 难度菜单键盘导航
  StartScreen screen; // 当前所在菜单界面
  StartAction action; // 主菜单动作：Draw/Update 阶段由按钮产生，Update 消费执行
  int diffAction;     // 难度菜单动作：-1=无、0/1/2=按该难度开始游戏
  // 难度菜单的服务模式：由主菜单哪个入口进入（PLAY=主线 100 关，
  // INFINITE=无尽模式），难度一经选择即按该模式创建对应首场景。
  StartAction pendingMode;
  // ── 标题小猫彩蛋（见 DrawTitleCat）──────────────────────────────────
  Texture2D catTexture; // icon.png 小猫贴图（onEnter 加载，onExit 释放）
  bool catValid;        // 贴图是否加载成功（失败时绘制静默跳过）
  int catLetter;        // 小猫所坐字母索引（标题字符下标，onEnter 随机）
  float catTime;        // 彩蛋动画计时（秒），驱动起伏
} StartData;

static void StartEnter(GameScene *self) {
  StartData *d = (StartData *)self->data;
  // 主菜单 BGM：进入菜单即切换到主题曲（CatET.mp3）
  GameAppSetMusicTrack((GameApp *)d->app, MUSIC_TRACK_MENU);
  // 进入开始菜单即开启新的一局：重置跨关卡生命值继承（下一关从满血开始）
  ((GameApp *)d->app)->playerHealth = 0.0f;
  // 新的一局：清空错词本/间隔重复记录（保留数组与词库绑定，见
  // systems/study_tracker）
  if (d->app->study)
    StudyReset(d->app->study);
  d->screen = START_SCREEN_MAIN;
  d->action = START_ACTION_NONE;
  d->pendingMode = START_ACTION_PLAY;
  d->diffAction = -1;
  MenuNavInit(&d->nav, START_ITEM_COUNT);
  MenuNavInit(&d->diffNav, DIFF_ITEM_COUNT);
  // 标题小猫彩蛋：加载 icon.png（16×16 像素画，点采样保持锐利）并随机挑一个
  // 标题字母落座——每次启动游戏小猫所在字母都不同；加载失败时 catValid=false，
  // 绘制阶段静默跳过（与全项目资源失败语义一致）。
  d->catTexture = LoadEmbeddedTexture("assets/sprites/icon.png");
  d->catValid = (d->catTexture.id != 0);
  if (d->catValid)
    SetTextureFilter(d->catTexture, TEXTURE_FILTER_POINT);
  const int titleLen = (int)strlen(kTitleText);
  d->catLetter = (titleLen > 0) ? genRandomNum(titleLen) : 0;
  d->catTime = 0.0f;
}

static void StartUpdate(GameScene *self, float dt) {
  StartData *d = (StartData *)self->data;
  // 标题小猫彩蛋计时：驱动小猫轻微上下起伏（纯装饰）
  d->catTime += dt;

  // ── 难度选择二级菜单 ─────────────────────────────────────────────
  if (d->screen == START_SCREEN_DIFFICULTY) {
    const int prevSelected = d->diffNav.selected;
    MenuAction act = MenuNavUpdate(&d->diffNav);
    // 选中项切换（W/S/↑↓）或确认/返回（Z/X）时播放 UI 音效
    if (d->diffNav.selected != prevSelected || act == MENU_ACTION_CONFIRM ||
        act == MENU_ACTION_BACK) {
      GameAppPlaySound(d->app, d->app->uiSound, d->app->uiSoundValid);
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

    // 消费难度动作（键盘确认写入），统一在此执行：
    // 按入口模式（主线/无尽）与所选难度创建对应首场景（经转场）
    if (d->diffAction >= 0) {
      if (d->pendingMode == START_ACTION_INFINITE) {
        // 无尽模式：自建无尽答题场景（难度决定词库与惩罚，scene_infinite）
        GameStackReplace(
            self->owner,
            TransitionSceneCreate(d->app,
                                  InfiniteSceneCreate(d->app, d->diffAction)));
      } else {
        // 主线模式：平台跳跃第 1 关，难度随 LevelFlow 传递
        GameStackReplace(
            self->owner,
            TransitionSceneCreate(
                d->app, LevelFlowCreateScene(d->app, LEVEL_TYPE_PLATFORM, 1,
                                             d->diffAction)));
      }
      return;
    }
    return;
  }

  // ── 主菜单 ──────────────────────────────────────────────────────
  const int prevSelected = d->nav.selected;
  MenuAction act = MenuNavUpdate(&d->nav);
  // 选中项切换（键盘 W/S/↑↓）或确认（Z）时播放 UI 音效
  if (d->nav.selected != prevSelected || act == MENU_ACTION_CONFIRM) {
    GameAppPlaySound(d->app, d->app->uiSound, d->app->uiSoundValid);
  }
  if (act == MENU_ACTION_CONFIRM) {
    d->action = kStartActions[d->nav.selected];
  }

  // 消费动作（全部来自键盘确认，此处统一执行切换/退出）
  switch (d->action) {
  case START_ACTION_PLAY:
  case START_ACTION_INFINITE:
    // 两个模式入口共用难度选择二级菜单：先记录模式，选好难度后再开赛。
    // 难度菜单的 X 返回会回到主菜单，不产生歧义。
    d->pendingMode = d->action;
    d->screen = START_SCREEN_DIFFICULTY;
    MenuNavInit(&d->diffNav, DIFF_ITEM_COUNT);
    d->diffAction = -1;
    break;
  case START_ACTION_SETTINGS:
    // 设置作为覆盖层压栈：返回（X/ESC）自动弹回主菜单，
    // 不再在主菜单内自绘设置二级菜单（见 scenes/scene_settings）
    GameStackPush(self->owner, SettingsSceneCreate(d->app));
    break;
  case START_ACTION_QUIT:
    GameStackRequestQuit(self->owner);
    break;
  default:
    break;
  }
  d->action = START_ACTION_NONE;
}

// 主菜单各条目前置图标（Play / Infinite / Settings / Quit）
static const int kStartIcons[START_ITEM_COUNT] = {ICON_PLAYER_PLAY, ICON_REPEAT,
                                                  ICON_GEAR, ICON_EXIT};

// 主菜单按钮绘制（Play / Infinite / Settings / Quit）
static void DrawMainMenu(StartData *d) {
  const float btnW = 200;
  const float btnH = 42;
  const float gap = 12;

  // 卡片面板：包住四项按钮，位于最佳时间之下、底部提示之上
  const float panelW = btnW + 40;
  const float panelH =
      START_ITEM_COUNT * btnH + (START_ITEM_COUNT - 1) * gap + 28;
  const float panelX = (d->app->logicWidth - panelW) / 2;
  const float panelY = d->app->logicHeight / 2.f - 22;
  UiThemePanel((Rectangle){panelX, panelY, panelW, panelH});

  const float btnX = panelX + (panelW - btnW) / 2;
  const float btnY = panelY + 14;

  for (int i = 0; i < START_ITEM_COUNT; i++) {
    Rectangle rec = {
        .x = btnX, .y = btnY + i * (btnH + gap), .width = btnW, .height = btnH};

    // 主题按钮：带图标 + 键盘选中高亮（见 tools/ui_theme）。
    // 鼠标已在 UiThemeApply 中经 GuiLock 全局禁用，此处只负责绘制，
    // 返回值恒为 false：选中（W/S/↑↓）与确认（Z）全部由 MenuNav 驱动。
    (void)UiThemeButton(rec, kStartIcons[i], kStartLabels[i],
                        i == d->nav.selected);
  }
}

// 难度选择各条目前置图标（Easy / Normal / Hard）
static const int kDiffIcons[DIFF_ITEM_COUNT] = {ICON_HEART, ICON_STAR,
                                                ICON_DEMON};

// 难度选择菜单按钮绘制（Easy / Normal / Hard；X 键返回主菜单）
static void DrawDifficultyMenu(StartData *d) {
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 标题置于副标题之下、按钮之上，避免与标题/副标题重叠；
  // 无尽模式与主线模式共用难度菜单，标题随入口模式提示当前目标
  const char *diffTitle = (d->pendingMode == START_ACTION_INFINITE)
                              ? "Infinite Difficulty"
                              : "Select Difficulty";
  const int dtSize = 20; // “Infinite Difficulty”/“Select Difficulty”标题字号
                         // （22 略宽，480 宽度下显示不全，降到 20）
  const int titleSize = START_TITLE_SIZE; // 与 StartDraw 中主标题字号一致
  const int subSize = 20;                 // 与 StartDraw 中副标题字号一致
  const int dtY = screenH / 4 + titleSize / 2 + 12 + subSize + 20;
  GameAppDrawText(d->app, diffTitle,
                  (screenW - GameAppMeasureText(d->app, diffTitle, dtSize)) / 2,
                  dtY, dtSize, DARKGRAY);

  const float btnW = 200;
  const float btnH = 44;
  const float gap = 14;

  // 卡片面板：包住三项难度按钮，位于标题之下、底部提示之上
  const float panelW = btnW + 40;
  const float panelH =
      DIFF_ITEM_COUNT * btnH + (DIFF_ITEM_COUNT - 1) * gap + 28;
  const float panelX = (screenW - panelW) / 2;
  const float panelY = screenH / 2.f + 6;
  UiThemePanel((Rectangle){panelX, panelY, panelW, panelH});

  const float btnX = panelX + (panelW - btnW) / 2;
  const float btnY = panelY + 14;

  for (int i = 0; i < DIFF_ITEM_COUNT; i++) {
    Rectangle rec = {
        .x = btnX, .y = btnY + i * (btnH + gap), .width = btnW, .height = btnH};

    // 主题按钮：带图标 + 键盘选中高亮（见 tools/ui_theme）；
    // 同上：鼠标已全局禁用，只绘制不接收点击，选择与确认由 MenuNav 驱动。
    (void)UiThemeButton(rec, kDiffIcons[i], kDiffLabels[i],
                        i == d->diffNav.selected);
  }
}

// 标题小猫彩蛋：把 icon.png 里的小猫放在标题某个字母上方（字母由 StartEnter
// 随机选定，每次启动游戏都不同）。逐字符测量前缀宽度定位字母（不假设等宽），
// 贴图按 GAME_SCALE 整数放大以保持像素锐利，并伴随轻微上下起伏。
static void DrawTitleCat(const StartData *d, int titleX, int titleY) {
  if (!d->catValid || d->catLetter < 0)
    return;
  const int titleLen = (int)strlen(kTitleText);
  if (d->catLetter >= titleLen)
    return;

  // 小猫所在字母的水平区间 = 前缀宽度 + 该字母自身宽度
  char prefix[16] = {0};
  for (int i = 0; i < d->catLetter; i++)
    prefix[i] = kTitleText[i];
  prefix[d->catLetter] = '\0';
  const char letter[2] = {kTitleText[d->catLetter], '\0'};
  const int prefixW = GameAppMeasureText(d->app, prefix, START_TITLE_SIZE);
  const int letterW = GameAppMeasureText(d->app, letter, START_TITLE_SIZE);

  const float scale = GAME_SCALE;
  const float catW = (float)d->catTexture.width * scale;
  const float catH = (float)d->catTexture.height * scale;
  const float centerX = (float)titleX + (float)prefixW + (float)letterW / 2.0f;
  const float bob =
      sinf(d->catTime * 2.0f * PI * START_TITLE_BOB_HZ) * START_TITLE_BOB_PX;
  const Vector2 pos = {centerX - catW / 2.0f,
                       (float)titleY - catH + START_TITLE_SINK + bob};
  DrawTextureEx(d->catTexture, pos, 0.0f, scale, WHITE);
}

static void StartDraw(GameScene *self) {
  StartData *d = (StartData *)self->data;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 标题（使用全局像素字体，与整体 UI 风格一致）
  const int titleSize = START_TITLE_SIZE;
  const int titleX =
      (screenW - GameAppMeasureText(d->app, kTitleText, titleSize)) / 2;
  const int titleY = screenH / 4 - titleSize / 2;
  GameAppDrawText(d->app, kTitleText, titleX, titleY, titleSize, DARKGRAY);
  // 标题小猫彩蛋：画在标题之后，让小猫叠压住字母顶端（随机字母每次启动不同）
  DrawTitleCat(d, titleX, titleY);

  const char *subtitle = "CET Words Challenge";
  const int subSize = 20;
  GameAppDrawText(d->app, subtitle,
                  (screenW - GameAppMeasureText(d->app, subtitle, subSize)) / 2,
                  screenH / 4 + titleSize / 2 + 12, subSize, GRAY);

  // 最佳通关时间（主菜单显示）：来自隐式全局计时器，仅成功通关才持久化
  // 记录（save.json），更优的数据会替换旧记录；尚无记录时显示占位 "--:--"。
  if (d->screen == START_SCREEN_MAIN) {
    const int bestSize = 16;
    const int bestY = screenH / 4 + titleSize / 2 + 12 + subSize + 18;
    const char *bestText =
        (d->app->bestTime >= 0.0f)
            ? TextFormat("Best Time %02d:%02d", (int)d->app->bestTime / 60,
                         (int)d->app->bestTime % 60)
            : "Best Time --:--";
    GameAppDrawText(d->app, bestText,
                    (screenW - GameAppMeasureText(d->app, bestText, bestSize)) /
                        2,
                    bestY, bestSize, DARKGRAY);
  }

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
  StartData *d = (StartData *)self->data;
  // 释放标题小猫彩蛋贴图（与 StartEnter 的加载一一配对）
  if (d->catValid)
    UnloadTexture(d->catTexture);
}

GameScene *StartSceneCreate(GameApp *app) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  StartData *data = (StartData *)calloc(1, sizeof(StartData));
  data->app = app;
  data->diffAction = -1;

  scene->name = "StartScene";
  scene->data = data;
  // 被设置覆盖层（SettingsScene）压栈时仍绘制，作为其半透明遮罩的底层画面
  scene->flags = GAME_SCENE_DRAW_WHEN_HIDDEN;
  scene->pauseable = false; // 开始界面不允许调出暂停画面
  scene->onEnter = StartEnter;
  scene->onUpdate = StartUpdate;
  scene->onDraw = StartDraw;
  scene->onExit = StartExit;
  // onPause / onResume 暂不需要，保持 NULL
  return scene;
}
