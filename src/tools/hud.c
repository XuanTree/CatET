#include "game.h"
#include <stdio.h>

// ─────────────────────────────────────────────────────────────────────────────
// 全局关卡 HUD 绘制工具实现：逻辑屏幕坐标固定绘制，供各场景在相机之外调用。
// 原实现内联在 scene_test.c 的 DrawHud / scene_maze.c 的 DrawHud 中，
// 抽离为本模块后各场景只需一行调用即可复用。
// ─────────────────────────────────────────────────────────────────────────────

void HudDrawHealthBar(const GameApp *app, float health, float maxHealth) {
  const float margin = 12.0f;
  const int fontSize = 16;
  const int screenH = app->logicHeight;

  // 左下角：生命值可视化进度条（颜色随剩余血量变化）。
  // bar 起点预留左侧 "HP" 标签空间，避免 raygui 左侧文本绘制到屏幕外。
  const float barW = 150.0f;
  const float barH = 16.0f;
  const float labelW = 24.0f; // "HP" 标签宽 + 间距
  const float barX = margin + labelW;
  const float barY = (float)screenH - margin - barH;
  const Rectangle hpBounds = {
      .x = barX, .y = barY, .width = barW, .height = barH};

  char hpText[16];
  snprintf(hpText, sizeof(hpText), "%d/%d", (int)health, (int)maxHealth);
  float hpValue = health;
  const float hpRatio = (maxHealth > 0.0f) ? health / maxHealth : 0.0f;
  const int prevTextSize = GuiGetStyle(DEFAULT, TEXT_SIZE);
  const int prevBarColor = GuiGetStyle(PROGRESSBAR, BASE_COLOR_PRESSED);
  // 血量 >50% 绿色，25%~50% 橙色，<=25% 红色
  if (hpRatio <= 0.25f) {
    GuiSetStyle(PROGRESSBAR, BASE_COLOR_PRESSED, 0xe74c3cff);
  } else if (hpRatio <= 0.50f) {
    GuiSetStyle(PROGRESSBAR, BASE_COLOR_PRESSED, 0xe67e22ff);
  } else {
    GuiSetStyle(PROGRESSBAR, BASE_COLOR_PRESSED, 0x27ae60ff);
  }
  GuiSetStyle(DEFAULT, TEXT_SIZE, fontSize);
  GuiProgressBar(hpBounds, "HP", hpText, &hpValue, 0.0f, maxHealth);
  GuiSetStyle(DEFAULT, TEXT_SIZE, prevTextSize);
  GuiSetStyle(PROGRESSBAR, BASE_COLOR_PRESSED, prevBarColor);
}

void HudDrawLevel(const GameApp *app, int level) {
  const float margin = 12.0f;
  const int fontSize = 16;

  // 左上角：当前关卡编号
  char levelText[24];
  snprintf(levelText, sizeof(levelText), "Level : %d", level);
  GameAppDrawText(app, levelText, (int)margin, (int)margin, fontSize, DARKGRAY);
}

void HudDrawTime(const GameApp *app, float timeSeconds) {
  const float margin = 12.0f;
  const int fontSize = 16;
  const int screenW = app->logicWidth;
  const int screenH = app->logicHeight;

  // 右下角：当前时间（mm:ss，右对齐）
  const int totalSec = (int)timeSeconds;
  char timeText[32];
  snprintf(timeText, sizeof(timeText), "Time %02d:%02d", totalSec / 60,
           totalSec % 60);
  const int timeW = GameAppMeasureText(app, timeText, fontSize);
  GameAppDrawText(app, timeText, screenW - (int)margin - timeW,
                  screenH - (int)margin - fontSize, fontSize, DARKGRAY);
}

void HudDrawEscHint(const GameApp *app) {
  const float margin = 12.0f;
  const int fontSize = 16;
  const int screenW = app->logicWidth;

  // 右上角：方框内含 ESC 提示（提示玩家按 ESC 暂停）
  const char *escText = "ESC";
  const int escW = GameAppMeasureText(app, escText, fontSize);
  const float boxW = (float)escW + 20.0f;
  const float boxH = (float)fontSize + 12.0f;
  const float boxX = (float)screenW - margin - boxW;
  const float boxY = margin;
  DrawRectangle((int)boxX, (int)boxY, (int)boxW, (int)boxH, Fade(BLACK, 0.55f));
  DrawRectangleLines((int)boxX, (int)boxY, (int)boxW, (int)boxH, DARKGRAY);
  GameAppDrawText(app, escText, (int)(boxX + (boxW - (float)escW) * 0.5f),
                  (int)(boxY + (boxH - (float)fontSize) * 0.5f), fontSize,
                  WHITE);
}

// 单词选项行自适应布局（见 hud.h 说明）：返回实际字号并填好各框矩形。
// 框宽随词长浮动；整行在 availW 内居中，放不下时先缩字号再试。
int HudLayoutWordRow(const GameApp *app, const char *const *words, int count,
                     float availW, int gap, int padX, int baseFontSize,
                     int minFontSize, float minBoxW, float boxH, float y,
                     Rectangle *outRects) {
  if (!app || !words || count <= 0 || !outRects)
    return baseFontSize;
  if (minFontSize <= 0)
    minFontSize = 8;
  int fs = (baseFontSize > minFontSize) ? baseFontSize : minFontSize;

  // 试排：总宽 = Σ max(minBoxW, 词宽+2*padX) + gap*(count-1)。
  // 放不进可用宽则逐档缩字号（保持同一行内字号一致），直到放下或到下限。
  float total = 0.0f;
  for (;;) {
    total = 0.0f;
    for (int i = 0; i < count; i++) {
      const char *w = words[i] ? words[i] : "";
      float bw = (float)GameAppMeasureText(app, w, fs) + (float)(padX * 2);
      if (bw < minBoxW)
        bw = minBoxW;
      total += bw;
    }
    total += (float)gap * (float)(count - 1);
    if (total <= availW || fs <= minFontSize)
      break;
    fs -= 2;
  }

  // 生成各框矩形：优先整体居中；极端超宽时从左侧排布保证互不重叠
  float x = (availW - total) * 0.5f;
  if (x < 0.0f)
    x = 0.0f;
  for (int i = 0; i < count; i++) {
    const char *w = words[i] ? words[i] : "";
    float bw = (float)GameAppMeasureText(app, w, fs) + (float)(padX * 2);
    if (bw < minBoxW)
      bw = minBoxW;
    outRects[i] = (Rectangle){.x = x, .y = y, .width = bw, .height = boxH};
    x += bw + (float)gap;
  }
  return fs;
}
