#ifndef TOOLS_MENU_H
#define TOOLS_MENU_H

#pragma once
#include <stdbool.h>

// ─────────────────────────────────────────────────────────────────────────────
// 通用键盘导航菜单（Menu Navigation）
//   为开始界面 / 暂停界面 / 设置界面提供统一的键盘操作（对应
//   docs/game_instructions.md：通过移动键选择选项，按 z 确认，按 x 返回）：
//   - 上/下（W/S 或 ↑/↓ 方向键）在选项间移动，到边界循环
//   - Z 键确认当前选中项（返回 MENU_ACTION_CONFIRM）
//   - X 键返回上级菜单（返回 MENU_ACTION_BACK，顶级菜单可忽略）
//   选中项由外部在绘制时高亮，例如配合 raygui 的 GuiSetState(STATE_FOCUSED)。
// ─────────────────────────────────────────────────────────────────────────────

typedef enum MenuAction {
  MENU_ACTION_NONE = 0, // 本帧无动作
  MENU_ACTION_CONFIRM,  // Z：确认当前选中项
  MENU_ACTION_BACK,     // X：返回上级菜单
} MenuAction;

typedef struct MenuNav {
  int selected; // 当前选中索引（0 ~ count-1）
  int count;    // 选项数量
  bool wrap;    // 到边界时是否循环移动（默认 true）
} MenuNav;

// 初始化：selected 归零，设置选项数量（count <= 0 时导航不可用）
void MenuNavInit(MenuNav *nav, int count);

// 每帧调用：读取键盘输入，更新 selected，返回本帧触发的动作
MenuAction MenuNavUpdate(MenuNav *nav);

#endif // TOOLS_MENU_H
