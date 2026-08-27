#include "game.h"

void MenuNavInit(MenuNav *nav, int count) {
  nav->selected = 0;
  nav->count = (count > 0) ? count : 0;
  nav->wrap = true;
}

MenuAction MenuNavUpdate(MenuNav *nav) {
  if (nav->count <= 0)
    return MENU_ACTION_NONE;

  // 上移：W 或 ↑
  if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
    nav->selected--;
    if (nav->selected < 0)
      nav->selected = nav->wrap ? nav->count - 1 : 0;
  }
  // 下移：S 或 ↓
  if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
    nav->selected++;
    if (nav->selected >= nav->count)
      nav->selected = nav->wrap ? 0 : nav->count - 1;
  }
  // 确认：Z
  if (IsKeyPressed(KEY_Z))
    return MENU_ACTION_CONFIRM;
  // 返回上级菜单：X
  if (IsKeyPressed(KEY_X))
    return MENU_ACTION_BACK;

  return MENU_ACTION_NONE;
}
