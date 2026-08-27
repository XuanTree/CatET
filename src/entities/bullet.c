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

// ─────────────────────────────────────────────────────────────────────────────
// 弹幕 pattern：敌怪每次攻击随机选择一种，数量也随机，增加玩法多样性。
// 4 种 pattern 均由「初始速度排布」构成（无需逐帧特殊状态），统一用
// InitBullet 生成；复用槽位前释放旧贴图，避免同一次战斗内 GPU 纹理泄漏。
// ─────────────────────────────────────────────────────────────────────────────

BulletPattern BulletPatternRoll(void) {
  return (BulletPattern)genRandomNum(BULLET_PATTERN_COUNT);
}

// 弹幕扇形总散布角（弧度）
#define BULLET_SPREAD_TIGHT 0.5f       // 瞄准扇形总散布角
#define BULLET_SPREAD_WIDE 1.6f        // 弹幕雨总散布角（宽）
#define BULLET_CROSS_LINE_OFFSET 0.06f // 十字每条对角线的内部散布

// 按 pattern 生成弹幕（见 bullet.h 注释）。
int BulletPatternFire(Bullet *bullets, int maxBullets, BulletPattern pattern,
                      Vector2 origin, Vector2 target, int count, float speed,
                      float damage) {
  if (!bullets || maxBullets <= 0)
    return 0;
  if (count < 1)
    count = 1;
  if (count > maxBullets)
    count = maxBullets;

  // 复用槽位前释放旧贴图（上一波弹幕可能占用过这些槽位）
  for (int i = 0; i < count; i++)
    if (bullets[i].bulletTexture.id != 0)
      UnloadTexture(bullets[i].bulletTexture);

  int spawned = 0;
  switch (pattern) {
  case BULLET_PATTERN_AIMED: {
    // 瞄准扇形：以目标（玩家）方向为中心均匀散射
    float base = atan2f(target.y - origin.y, target.x - origin.x);
    for (int i = 0; i < count; i++) {
      float angle =
          base + (i - (count - 1) * 0.5f) * BULLET_SPREAD_TIGHT / (float)count;
      Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
      InitBullet(&bullets[spawned], origin, vel, damage);
      spawned++;
    }
    break;
  }
  case BULLET_PATTERN_RING: {
    // 环形：以发射点为中心 360° 均匀分布
    for (int i = 0; i < count; i++) {
      float angle = 2.0f * PI * (float)i / (float)count;
      Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
      InitBullet(&bullets[spawned], origin, vel, damage);
      spawned++;
    }
    break;
  }
  case BULLET_PATTERN_RAIN: {
    // 弹幕雨：宽扇形向下倾泻，覆盖玩家走位区域
    float base = PI * 0.5f; // 正下方
    for (int i = 0; i < count; i++) {
      float angle =
          base + (i - (count - 1) * 0.5f) * BULLET_SPREAD_WIDE / (float)count;
      Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
      InitBullet(&bullets[spawned], origin, vel, damage);
      spawned++;
    }
    break;
  }
  case BULLET_PATTERN_CROSS: {
    // 十字斜扫：沿 0°/45°/90°/135° 四条对角线发射，每条内带微小散布
    int perLine = (count + 3) / 4;
    for (int k = 0; k < 4; k++) {
      float base = PI * (float)k / 4.0f;
      for (int j = 0; j < perLine && spawned < count; j++) {
        float angle =
            base + (j - (perLine - 1) * 0.5f) * BULLET_CROSS_LINE_OFFSET;
        Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
        InitBullet(&bullets[spawned], origin, vel, damage);
        spawned++;
      }
    }
    break;
  }
  default:
    break;
  }
  return spawned;
}
