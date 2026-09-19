#include "game.h"

// ─────────────────────────────────────────────────────────────────────────────
// 剧情覆盖层实现（见 include/systems/story.h）：
//   - 打字机逐字输出：每 STORY_CHAR_TIME 秒输出一个字符（与 scene_battle
//     敌怪对话系统的吐字节奏一致）；输出完毕后 finished=true，文本保留。
//   - 绘制为白底黑字文本框，固定锚定在关卡世界的出生点处（世界坐标），
//     文本按词换行，避免长句溢出屏幕。
// ─────────────────────────────────────────────────────────────────────────────

#define STORY_CHAR_TIME 0.04f // 逐字输出每字间隔（秒，与战斗对话一致）
#define STORY_PAD_X 10        // 文本框水平内边距（像素）
#define STORY_PAD_Y 8         // 文本框垂直内边距（像素）
#define STORY_LINE_GAP 4      // 行间距（像素）

void StoryOverlayClear(StoryOverlay *story) {
  if (story == NULL)
    return;
  memset(story, 0, sizeof(*story));
}

void StoryOverlayStart(StoryOverlay *story, const char *text) {
  if (story == NULL)
    return;
  StoryOverlayClear(story);
  if (text == NULL || text[0] == '\0')
    return; // 无剧情（Boss 关 / 已通关一次后）：保持 inactive
  snprintf(story->text, sizeof(story->text), "%s", text);
  story->charShown = 0;
  story->typeTimer = 0.f;
  story->finished = false;
  story->active = true;
}

void StoryOverlayUpdate(StoryOverlay *story, float dt) {
  if (story == NULL || !story->active)
    return;
  const int len = (int)strlen(story->text);
  if (story->charShown >= len) {
    story->finished = true;
    return;
  }
  story->typeTimer += dt;
  while (story->typeTimer >= STORY_CHAR_TIME && story->charShown < len) {
    story->typeTimer -= STORY_CHAR_TIME;
    story->charShown++;
  }
  if (story->charShown >= len)
    story->finished = true;
}

bool StoryOverlayIsFinished(const StoryOverlay *story) {
  return (story != NULL) && story->active && story->finished;
}

// 按词把文本折行装入 lines（最多 STORY_MAX_LINES 行），返回实际行数。
// 单行宽度超过 maxWidth 时从词边界换行；单个词自身超宽时允许该行溢出。
static int StoryWrapLines(const GameApp *app, const char *text, int maxWidth,
                          int fontSize,
                          char lines[STORY_MAX_LINES][STORY_TEXT_MAX]) {
  int count = 0;
  lines[0][0] = '\0';
  int i = 0;
  const int n = (int)strlen(text);
  while (i < n && count < STORY_MAX_LINES) {
    while (i < n && text[i] == ' ')
      i++; // 跳过词间空格
    if (i >= n)
      break;
    const int ws = i;
    while (i < n && text[i] != ' ')
      i++;
    int wlen = i - ws;
    if (wlen > STORY_TEXT_MAX - 1)
      wlen = STORY_TEXT_MAX - 1;
    char word[STORY_TEXT_MAX];
    memcpy(word, text + ws, (size_t)wlen);
    word[wlen] = '\0';

    char cand[STORY_TEXT_MAX];
    if (lines[count][0] == '\0')
      snprintf(cand, sizeof(cand), "%s", word);
    else
      snprintf(cand, sizeof(cand), "%s %s", lines[count], word);

    if (lines[count][0] != '\0' &&
        GameAppMeasureText(app, cand, fontSize) > maxWidth) {
      // 当前行放不下该词：换行后另起一行
      count++;
      if (count >= STORY_MAX_LINES) {
        count = STORY_MAX_LINES - 1;
        break;
      }
      snprintf(lines[count], STORY_TEXT_MAX, "%s", word);
    } else {
      snprintf(lines[count], STORY_TEXT_MAX, "%s", cand);
    }
  }
  return count + 1;
}

// 取已输出部分文本（打字机进度，避免每帧重复计算）。
static void StoryShownText(const StoryOverlay *story, char *out, int outSize) {
  const int len = (int)strlen(story->text);
  int n = story->charShown;
  if (n > len)
    n = len;
  if (n > outSize - 1)
    n = outSize - 1;
  memcpy(out, story->text, (size_t)n);
  out[n] = '\0';
}

int StoryOverlayHeight(const GameApp *app, const StoryOverlay *story,
                       int maxWidth, int fontSize) {
  if (story == NULL || !story->active)
    return 0;
  char shown[STORY_TEXT_MAX];
  StoryShownText(story, shown, (int)sizeof(shown));
  char lines[STORY_MAX_LINES][STORY_TEXT_MAX];
  const int lineCount = StoryWrapLines(app, shown, maxWidth, fontSize, lines);
  const int lineH = fontSize + STORY_LINE_GAP;
  return lineCount * lineH - STORY_LINE_GAP + STORY_PAD_Y * 2;
}

void StoryOverlayDrawAnchored(const GameApp *app, const StoryOverlay *story,
                              float anchorX, float anchorY, int maxWidth,
                              int fontSize, float minX, float maxX) {
  if (story == NULL || !story->active)
    return;
  char shown[STORY_TEXT_MAX];
  StoryShownText(story, shown, (int)sizeof(shown));
  char lines[STORY_MAX_LINES][STORY_TEXT_MAX];
  const int lineCount = StoryWrapLines(app, shown, maxWidth, fontSize, lines);
  const int lineH = fontSize + STORY_LINE_GAP;
  const int boxW = maxWidth + STORY_PAD_X * 2;
  const int boxH = lineCount * lineH - STORY_LINE_GAP + STORY_PAD_Y * 2;

  // 水平居中于锚点，左缘钳制在 [minX, maxX - boxW] 内
  float boxX = anchorX - (float)boxW * 0.5f;
  if (boxX < minX)
    boxX = minX;
  const float maxBoxX = maxX - (float)boxW;
  if (maxBoxX > minX && boxX > maxBoxX)
    boxX = maxBoxX;
  // 底边对齐锚点（文本框显示在锚点上方）
  const int bx = (int)boxX;
  const int by = (int)anchorY - boxH;

  // 白底黑字（与战斗对话一致），半透明白底保证关卡背景上仍可阅读
  DrawRectangle(bx, by, boxW, boxH, Fade(WHITE, 0.85f));
  DrawRectangleLines(bx, by, boxW, boxH, Fade(BLACK, 0.6f));
  for (int i = 0; i < lineCount; i++) {
    GameAppDrawText(app, lines[i], bx + STORY_PAD_X,
                    by + STORY_PAD_Y + i * lineH, fontSize, BLACK);
  }
}
