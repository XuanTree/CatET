#include "game.h"
// 弹幕尺寸：贴图 8×8 × GAME_SCALE（与玩家/平台同比例）
#define BULLET_WIDTH (8.f * GAME_SCALE)
#define BULLET_HEIGHT (8.f * GAME_SCALE)
// 弹幕存活时长（秒）：超时自动失效，避免弹幕无限飞行
#define BULLET_MAX_LIFETIME 5.f

// 初始化子弹：设定出生位置、速度与伤害，置 isActive
void InitBullet(Bullet *bullet, Vector2 position, Vector2 velocity,
                float damage) {
  if (!bullet)
    return;

  bullet->position = position;
  bullet->velocity = velocity;
  bullet->size = (Vector2){.x = BULLET_WIDTH, .y = BULLET_HEIGHT};
  bullet->damage = damage;
  bullet->lifetime = BULLET_MAX_LIFETIME;
  bullet->isActive = true;
  // 每颗子弹独立加载贴图；后续若引入对象池，应改为共享同一纹理
  bullet->bulletTexture = LoadTexture(
      TextFormat("%sassets/sprites/bullet.png", GetApplicationDirectory()));
}

// 每帧更新：位移、衰减存活时长，超时自动失效
void UpdateBullet(Bullet *bullet, float dt) {
  if (!bullet || !bullet->isActive)
    return;

  bullet->position.x += bullet->velocity.x * dt;
  bullet->position.y += bullet->velocity.y * dt;

  bullet->lifetime -= dt;
  if (bullet->lifetime <= 0.f)
    bullet->isActive = false;
}

// 绘制子弹：绘制尺寸与碰撞 bullet->size 严格一致
void DrawBullet(Bullet *bullet) {
  if (!bullet || !bullet->isActive)
    return;

  Rectangle src = {.x = 0.f,
                   .y = 0.f,
                   .width = (float)bullet->bulletTexture.width,
                   .height = (float)bullet->bulletTexture.height};
  Rectangle dest = {
      .x = bullet->position.x,
      .y = bullet->position.y,
      .width = bullet->size.x,
      .height = bullet->size.y,
  };
  DrawTexturePro(bullet->bulletTexture, src, dest, (Vector2){0.f, 0.f}, 0.f,
                 WHITE);
}
