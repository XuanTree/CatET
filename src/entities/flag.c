#include "entities/flag.h"

// 红旗几何常量（世界坐标像素）：旗杆高 64，红旗三角 34x22，底座 22x6。
#define FLAG_POLE_HEIGHT 64.0f
#define FLAG_POLE_WIDTH 4.0f
#define FLAG_TRI_WIDTH 34.0f
#define FLAG_TRI_HEIGHT 22.0f
#define FLAG_BASE_WIDTH 22.0f
#define FLAG_BASE_HEIGHT 6.0f
// 碰撞盒：覆盖旗杆高度、略宽于旗杆，便于玩家触碰判定。
#define FLAG_HIT_WIDTH 30.0f

void InitFlag(Flag *flag, Vector2 base) {
  if (!flag)
    return;
  flag->base = base;
  flag->hitbox = (Rectangle){
      .x = base.x - FLAG_HIT_WIDTH * 0.5f,
      .y = base.y - FLAG_POLE_HEIGHT,
      .width = FLAG_HIT_WIDTH,
      .height = FLAG_POLE_HEIGHT,
  };
}

void DrawFlag(const Flag *flag) {
  if (!flag)
    return;
  Vector2 b = flag->base;
  float topY = b.y - FLAG_POLE_HEIGHT;
  float poleLeft = b.x - FLAG_POLE_WIDTH * 0.5f;

  // 旗杆
  DrawRectangle((int)poleLeft, (int)topY, (int)FLAG_POLE_WIDTH,
                (int)FLAG_POLE_HEIGHT, DARKGRAY);
  // 红旗三角（朝向右侧，随风吹向）
  DrawTriangle(
      (Vector2){poleLeft, topY}, (Vector2){poleLeft, topY + FLAG_TRI_HEIGHT},
      (Vector2){poleLeft + FLAG_TRI_WIDTH, topY + FLAG_TRI_HEIGHT * 0.5f}, RED);
  // 旗杆顶部圆点
  DrawCircleV((Vector2){b.x, topY}, FLAG_POLE_WIDTH * 0.5f, RED);
  // 底座
  DrawRectangle((int)(b.x - FLAG_BASE_WIDTH * 0.5f),
                (int)(b.y - FLAG_BASE_HEIGHT), (int)FLAG_BASE_WIDTH,
                (int)FLAG_BASE_HEIGHT, DARKGRAY);
}

bool FlagCheckCollision(const Flag *flag, Rectangle playerRect) {
  if (!flag)
    return false;
  return CheckCollisionRecs(flag->hitbox, playerRect);
}
