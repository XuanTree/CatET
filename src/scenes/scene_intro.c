#include "game.h"
#include <raylib.h>
#include <stdlib.h>

// ─────────────────────────────────────────────────────────────────────────────
// 启动名言转场场景（IntroScene）：
//   游戏启动后的首个场景，以「黑底白字」显示一句随机名言（dialogue.getStartText），
//   INTRO_SECONDS 秒后自动（或按 X / Z / Enter / Space 跳过）经通用过渡场景
//   （TransitionSceneCreate → scene_transition 的黑白遮罩动画）进入主菜单。
//   目的：给玩家一个安静的开场，避免一启动就进入菜单并播放 BGM 的突兀感。
//   本场景不声明音乐曲目（保持启动静音），BGM 从主菜单开始播放。
// ─────────────────────────────────────────────────────────────────────────────

#define INTRO_SECONDS 3.0f // 名言展示时长（秒），期间可按跳过键提前结束
// 正文字号：像素字基准 48 的整数倍缩放下更清晰；20 与主菜单副标题字号一致
#define INTRO_FONT_SIZE 20
#define INTRO_LINE_GAP 10  // 行间距（像素）
#define INTRO_MARGIN_X 48  // 文本块左右留白（像素），折行的最大可用宽度据此计算
#define INTRO_MAX_LINES 8  // 折行上限（名言最长约 150 字符，8 行足够）
#define INTRO_LINE_CAP 128 // 单行缓冲大小
#define INTRO_CLEAN_CAP 512 // 规范化后的整句缓冲大小

// 场景私有数据：栈持有并负责释放
typedef struct IntroData {
  GameApp *app; // 引用（不拥有；切换场景需传给目标场景工厂）
  Timer timer;  // 名言展示计时（onEnter 重置，onUpdate 累计）
  char lines[INTRO_MAX_LINES][INTRO_LINE_CAP]; // 折行后的名言文本
  int lineCount;                               // 实际行数（>=1）
  bool done;                                   // 是否已请求切换（防止重复入队）
} IntroData;

// 将 UTF-8 名言规范化为默认像素字体可渲染的 ASCII：
//   “ ” → "   ‘ ’ → '   — – → -
//   其余非 ASCII 字符（含中文）整体丢弃。
// 为什么需要：项目自定义像素字体缺失时会回退 raylib 默认字体（仅含 ASCII），
// 若不规范化，名言中的弯引号 / 破折号会渲染成缺字方块，影响观感。
static void SanitizeToAscii(const char *src, char *dst, size_t cap) {
  size_t j = 0;
  size_t i = 0;
  while (src[i] != '\0' && j + 1 < cap) {
    const unsigned char c = (unsigned char)src[i];
    if (c < 0x80) { // ASCII 原样保留
      dst[j++] = (char)c;
      i++;
      continue;
    }
    // 三字节 UTF-8（U+2013 ~ U+201D 等常用标点均在此区间）
    if ((c & 0xF0) == 0xE0 && src[i + 1] != '\0' && src[i + 2] != '\0') {
      const unsigned int cp =
          ((unsigned int)(c & 0x0F) << 12) |
          ((unsigned int)((unsigned char)src[i + 1] & 0x3F) << 6) |
          ((unsigned char)src[i + 2] & 0x3F);
      if (cp == 0x2018 || cp == 0x2019) {
        dst[j++] = '\'';
      } else if (cp == 0x201C || cp == 0x201D) {
        dst[j++] = '"';
      } else if (cp == 0x2013 || cp == 0x2014) {
        dst[j++] = '-';
      }
      i += 3;
      continue;
    }
    // 其它多字节序列（2/4 字节）：整体跳过
    int len = 1;
    if ((c & 0xE0) == 0xC0) {
      len = 2;
    } else if ((c & 0xF8) == 0xF0) {
      len = 4;
    }
    i += (size_t)len;
  }
  dst[j] = '\0';
}

// 将整句按空格折行到不超过 maxWidth 的多行（基于全局字体实测宽度）。
// 单词超过单行宽度时独占一行（不切分单词），行数上限 INTRO_MAX_LINES。
static void WrapToLines(GameApp *app, const char *text, IntroData *d,
                        int maxWidth) {
  for (int i = 0; i < INTRO_MAX_LINES; i++)
    d->lines[i][0] = '\0';
  d->lineCount = 1;

  const char *p = text;
  int line = 0;
  while (*p != '\0' && line < INTRO_MAX_LINES) {
    while (*p == ' ')
      p++;
    if (*p == '\0')
      break;

    // 取一个单词（以空格为界）
    char word[INTRO_LINE_CAP];
    int wl = 0;
    while (*p != '\0' && *p != ' ' && wl < INTRO_LINE_CAP - 1)
      word[wl++] = *p++;
    word[wl] = '\0';

    // 试探性拼接：空行直接放；否则「当前行 + 空格 + 单词」
    char cand[INTRO_LINE_CAP * 2];
    if (d->lines[line][0] == '\0')
      snprintf(cand, sizeof(cand), "%s", word);
    else
      snprintf(cand, sizeof(cand), "%s %s", d->lines[line], word);

    if (d->lines[line][0] == '\0' ||
        GameAppMeasureText(app, cand, INTRO_FONT_SIZE) <= maxWidth) {
      snprintf(d->lines[line], INTRO_LINE_CAP, "%s", cand);
    } else {
      // 放不下：换行，单词作为新行首
      line++;
      if (line >= INTRO_MAX_LINES)
        break;
      snprintf(d->lines[line], INTRO_LINE_CAP, "%s", word);
    }
  }
  d->lineCount = line + 1;
}

static void IntroEnter(GameScene *self) {
  IntroData *d = (IntroData *)self->data;
  // 重置计时器（不可直接置 startTime=0，否则首帧即判定超时）
  ResetTimer(&d->timer);
  d->done = false;

  // 随机取一句名言 → 规范化为 ASCII → 按屏幕宽度折行
  char clean[INTRO_CLEAN_CAP];
  SanitizeToAscii(getStartText(), clean, sizeof(clean));
  WrapToLines(d->app, clean, d, d->app->logicWidth - 2 * INTRO_MARGIN_X);
}

static void IntroUpdate(GameScene *self, float dt) {
  (void)dt; // Timer 基于 GetTime() 累计，与帧间隔无关
  IntroData *d = (IntroData *)self->data;
  if (d->done)
    return;

  UpdateTimer(&d->timer);

  // 跳过键：X 为主提示，兼容 Z / Enter / Space 便于操作
  const bool skip = IsKeyPressed(KEY_X) || IsKeyPressed(KEY_Z) ||
                    IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
  if (skip || GetElapsedTime(&d->timer) >= INTRO_SECONDS) {
    d->done = true;
    // 经通用过渡场景进入主菜单（过渡结束 Replace 到
    // StartScene，本场景随之销毁）。 延迟请求由 GameStackUpdate 帧首统一
    // flush，回调内调用是安全的。
    GameStackReplace(self->owner,
                     TransitionSceneCreate(d->app, StartSceneCreate(d->app)));
  }
}

static void IntroDraw(GameScene *self) {
  IntroData *d = (IntroData *)self->data;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 黑底白字：全屏不透明背景
  DrawRectangle(0, 0, screenW, screenH, BLACK);

  // 名言文本块整体垂直/水平居中
  const int lineH = INTRO_FONT_SIZE + INTRO_LINE_GAP;
  const int blockH =
      d->lineCount * INTRO_FONT_SIZE +
      (d->lineCount > 0 ? (d->lineCount - 1) * INTRO_LINE_GAP : 0);
  int y = (screenH - blockH) / 2;
  for (int i = 0; i < d->lineCount; i++) {
    const int w = GameAppMeasureText(d->app, d->lines[i], INTRO_FONT_SIZE);
    GameAppDrawText(d->app, d->lines[i], (screenW - w) / 2, y, INTRO_FONT_SIZE,
                    WHITE);
    y += lineH;
  }

  // 底部跳过提示（淡灰，不喧宾夺主）
  const char *hint = "Press X to skip";
  GameAppDrawText(d->app, hint,
                  (screenW - GameAppMeasureText(d->app, hint, 16)) / 2,
                  screenH - 24, 16, GRAY);
}

GameScene *IntroSceneCreate(GameApp *app) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  if (scene == NULL) {
    return NULL;
  }
  IntroData *data = (IntroData *)calloc(1, sizeof(IntroData));
  if (data == NULL) {
    free(scene);
    return NULL;
  }
  data->app = app;
  data->done = false;

  scene->name = "IntroScene";
  scene->data = data;
  scene->flags = GAME_SCENE_NONE; // 全屏不透明，不依赖下层绘制
  scene->pauseable = false;       // 启动过场不允许调出暂停画面
  scene->onEnter = IntroEnter;
  scene->onUpdate = IntroUpdate;
  scene->onDraw = IntroDraw;
  // onExit / onPause / onResume 本场景不需要，保持 NULL
  return scene;
}
