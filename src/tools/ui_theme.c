#include "game.h"

// ─────────────────────────────────────────────────────────────────────────────
// 全局 UI 主题实现（见 include/tools/ui_theme.h）
//   配色思路：浅色卡片 + 石板灰文字 + 琥珀色高亮（与小猫主题的柔和观感一致）。
//   raygui 中设置 DEFAULT 控件的 BASE 属性会传播到所有控件（见 GuiSetStyle），
//   因此只需在 DEFAULT 上设置一次即可统一全局；控件专属属性（如 BUTTON 的
//   边框宽度、TOGGLE 的打开态配色）在传播之后再单独覆盖。
// ─────────────────────────────────────────────────────────────────────────────

// ── 主题色板（0xRRGGBBAA）───────────────────────────────────────────────────
#define UI_COL_BORDER_NORMAL 0x94a3b8ff // 常态描边（slate-400）
#define UI_COL_BASE_NORMAL 0xfbfdffff   // 常态底色（近白，略亮于卡片底）
#define UI_COL_TEXT_NORMAL 0x334155ff   // 常态文字（slate-700）
// 高亮配色沿用 raygui 原有的淡蓝色系（选中/悬停），只统一非高亮部分的观感
#define UI_COL_BORDER_FOCUSED 0x5bb2d9ff  // 高亮描边（原 raygui 淡蓝）
#define UI_COL_BASE_FOCUSED 0xc9effeff    // 高亮底色（原 raygui 浅蓝）
#define UI_COL_TEXT_FOCUSED 0x6c9bbcff    // 高亮文字（原 raygui 蓝灰）
#define UI_COL_BORDER_PRESSED 0xea580cff  // 按下描边（orange-600）
#define UI_COL_BASE_PRESSED 0xfdba74ff    // 按下底色（orange-300）
#define UI_COL_TEXT_PRESSED 0x7c2d12ff    // 按下文字（orange-900）
#define UI_COL_BORDER_DISABLED 0xcbd5e1ff // 禁用描边（slate-300）
#define UI_COL_BASE_DISABLED 0xf1f5f9ff   // 禁用底色（slate-100）
#define UI_COL_TEXT_DISABLED 0x94a3b8ff   // 禁用文字（slate-400）
// 面板描边/分隔线取略深的 slate-300：既能在深色遮罩上勾出卡片轮廓，
// 又能在纯白底（如战斗对话框的 GuiGroupBox）上保持可见
#define UI_COL_LINE 0xcbd5e1ff
#define UI_COL_BACKGROUND 0xf1f5f9ff // 卡片/面板底色（slate-100，衬出按钮）

// Toggle 打开态配色（GuiToggle 在 active 时使用 PRESSED 三色）
#define UI_COL_TOGGLE_ON_BORDER 0x16a34aff // green-600
#define UI_COL_TOGGLE_ON_BASE 0xbbf7d0ff   // green-200
#define UI_COL_TOGGLE_ON_TEXT 0x14532dff   // green-900

// 键盘选中外描边颜色（与原 raygui FOCUSED 描边一致的淡蓝）
static const Color kFocusRingColor = {91, 178, 217, 255};

void UiThemeApply(void) {
  // 基础配色：设置 DEFAULT 会传播到所有控件（GuiSetStyle 内部实现）
  GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, UI_COL_BORDER_NORMAL);
  GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, UI_COL_BASE_NORMAL);
  GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, UI_COL_TEXT_NORMAL);
  GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, UI_COL_BORDER_FOCUSED);
  GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, UI_COL_BASE_FOCUSED);
  GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, UI_COL_TEXT_FOCUSED);
  GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, UI_COL_BORDER_PRESSED);
  GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, UI_COL_BASE_PRESSED);
  GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, UI_COL_TEXT_PRESSED);
  GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED, UI_COL_BORDER_DISABLED);
  GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED, UI_COL_BASE_DISABLED);
  GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED, UI_COL_TEXT_DISABLED);

  // 面板/卡片相关（DEFAULT 专属扩展属性）
  GuiSetStyle(DEFAULT, LINE_COLOR, UI_COL_LINE);
  GuiSetStyle(DEFAULT, BACKGROUND_COLOR, UI_COL_BACKGROUND);

  // 控件专属覆盖：按钮描边略粗（DEFAULT 配色传播会覆盖其默认值，故在其后设置）
  GuiSetStyle(BUTTON, BORDER_WIDTH, 2);

  // 开关打开态配色：GuiToggle 在 active 时使用 PRESSED
  // 三色（绿色表达「已开启」）
  GuiSetStyle(TOGGLE, BORDER_COLOR_PRESSED, UI_COL_TOGGLE_ON_BORDER);
  GuiSetStyle(TOGGLE, BASE_COLOR_PRESSED, UI_COL_TOGGLE_ON_BASE);
  GuiSetStyle(TOGGLE, TEXT_COLOR_PRESSED, UI_COL_TOGGLE_ON_TEXT);

  // ── 全局输入策略：UI 只认键盘，彻底与鼠标解耦 ─────────────────────────
  // GuiLock 后所有 raygui 控件跳过鼠标处理（raygui 内部每个控件的输入分支
  // 都写作 `(state != STATE_DISABLED) && !guiLocked`）：悬停不再高亮、按下
  // 不再变橙、点击不再触发，鼠标 tooltip 也一并失效。
  // 关键点是「绘制路径与输入无关」：常态外观、键盘选中时由
  // GuiSetState(STATE_FOCUSED) 施加的高亮、开关打开态的绿色照常呈现，因此
  // 界面视觉不受影响，只是交互一律由各场景的 MenuNav（W/S/↑↓ 移动，Z 确认，
  // X 返回）驱动。若要恢复鼠标交互，只需注释掉本行。
  GuiLock();
}

// 构造带图标的控件文本："#123#text"（raygui 图标语法；iconId < 0 时不加图标）。
// 使用局部缓冲而非 GuiIconText()，避免其静态缓冲区被多次调用互相覆盖。
static void BuildIconLabel(char *out, int outSize, int iconId,
                           const char *text) {
  if (text == NULL)
    text = "";
  if (iconId >= 0)
    snprintf(out, (size_t)outSize, "#%03d#%s", iconId, text);
  else
    snprintf(out, (size_t)outSize, "%s", text);
}

bool UiThemeButton(Rectangle bounds, int iconId, const char *text,
                   bool focused) {
  char label[192];
  BuildIconLabel(label, (int)sizeof(label), iconId, text);
  if (focused)
    GuiSetState(STATE_FOCUSED);
  const bool clicked = GuiButton(bounds, label) != 0;
  if (focused)
    GuiSetState(STATE_NORMAL);
  return clicked;
}

bool UiThemeToggle(Rectangle bounds, int iconId, const char *text, bool *active,
                   bool focused) {
  char label[192];
  BuildIconLabel(label, (int)sizeof(label), iconId, text);
  // 始终以 NORMAL 状态绘制，保证打开态的绿色不被 FOCUSED 高亮覆盖
  const bool changed = GuiToggle(bounds, label, active) != 0;
  if (focused) {
    const Rectangle ring = {bounds.x - 3.0f, bounds.y - 3.0f,
                            bounds.width + 6.0f, bounds.height + 6.0f};
    DrawRectangleLinesEx(ring, 2.0f, kFocusRingColor);
  }
  return changed;
}

void UiThemePanel(Rectangle bounds) { GuiPanel(bounds, NULL); }
