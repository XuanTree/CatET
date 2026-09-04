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
  bullet->bulletTexture = LoadEmbeddedTexture("assets/sprites/bullet.png");
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

// 弹幕视觉内容圆：贴图为 16×16 内居中的 12×12 圆形（左右上下各留 2px），
// 圆心与贴图中心重合，半径 = size.x * (12/16) / 2 = size.x * 0.375。
void BulletHitCircle(const Bullet *bullet, Vector2 *outCenter,
                     float *outRadius) {
  if (!bullet) {
    if (outCenter)
      *outCenter = (Vector2){0.f, 0.f};
    if (outRadius)
      *outRadius = 0.f;
    return;
  }
  const float radius = bullet->size.x * (12.0f / 16.0f) * 0.5f;
  if (outCenter)
    *outCenter = (Vector2){bullet->position.x + bullet->size.x * 0.5f,
                           bullet->position.y + bullet->size.y * 0.5f};
  if (outRadius)
    *outRadius = radius;
}

// ─────────────────────────────────────────────────────────────────────────────
// 弹幕 pattern：敌怪每次攻击随机选择一种（共 10 种），数量也随机，增加玩法
// 多样性。均由「初始速度/位置排布」构成（无需逐帧特殊状态），统一用
// InitBullet 生成；复用槽位前释放旧贴图，避免同一次战斗内 GPU 纹理泄漏。
// ─────────────────────────────────────────────────────────────────────────────

BulletPattern BulletPatternRoll(void) {
  return (BulletPattern)genRandomNum(BULLET_PATTERN_COUNT);
}

// 弹幕各 pattern 的散布/排布参数（弧度 / 像素）
#define BULLET_SPREAD_TIGHT 0.5f  // 瞄准窄扇形总散布角
#define BULLET_SPREAD_WIDE 1.6f   // 宽扇形总散布角（弹幕雨 / 瞄准大扇形）
#define BULLET_SPIRAL_STEP 0.28f  // 螺旋每颗递增角度（弧度）
#define BULLET_PINCER_ANGLE 0.5f  // 钳形两翼相对玩家的偏移角（弧度）
#define BULLET_PINCER_SPREAD 0.4f // 钳形每组内部总散布
#define BULLET_WALL_SPACING 24.f  // 弹幕墙相邻弹幕间距（像素）
#define BULLET_STAR_AXES 6        // 星形放射轴数
#define BULLET_STAR_SPREAD 0.6f   // 星形每轴内部总散布
#define BULLET_ARC_GAP 0.5f       // 三段弧相邻组偏移角（弧度）
#define BULLET_HAIL_SPACING 34.f  // 天降弹幕横向间距（像素）

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
    // 瞄准扇形：以玩家方向为中心窄扇形散射
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
  case BULLET_PATTERN_SPIRAL: {
    // 螺旋：基准朝玩家，角度随序号持续递增 → 形成螺旋射线
    float base = atan2f(target.y - origin.y, target.x - origin.x);
    float start = base - (count - 1) * 0.5f * BULLET_SPIRAL_STEP;
    for (int i = 0; i < count; i++) {
      float angle = start + (float)i * BULLET_SPIRAL_STEP;
      Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
      InitBullet(&bullets[spawned], origin, vel, damage);
      spawned++;
    }
    break;
  }
  case BULLET_PATTERN_PINCER: {
    // 钳形：朝玩家左右两侧各一组窄扇形，封锁两侧走位
    float base = atan2f(target.y - origin.y, target.x - origin.x);
    int left = count / 2;
    int right = count - left;
    for (int i = 0; i < left; i++) {
      float angle =
          base - BULLET_PINCER_ANGLE +
          (i - (left - 1) * 0.5f) * BULLET_PINCER_SPREAD / (float)left;
      Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
      InitBullet(&bullets[spawned], origin, vel, damage);
      spawned++;
    }
    for (int i = 0; i < right; i++) {
      float angle =
          base + BULLET_PINCER_ANGLE +
          (i - (right - 1) * 0.5f) * BULLET_PINCER_SPREAD / (float)right;
      Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
      InitBullet(&bullets[spawned], origin, vel, damage);
      spawned++;
    }
    break;
  }
  case BULLET_PATTERN_WALL: {
    // 弹幕墙：横向一排平行弹幕，整体朝玩家方向扫射
    float base = atan2f(target.y - origin.y, target.x - origin.x);
    Vector2 dir = {cosf(base) * speed, sinf(base) * speed};
    Vector2 perp = {cosf(base + PI * 0.5f), sinf(base + PI * 0.5f)};
    for (int i = 0; i < count; i++) {
      float off = (i - (count - 1) * 0.5f) * BULLET_WALL_SPACING;
      Vector2 pos = {origin.x + perp.x * off, origin.y + perp.y * off};
      InitBullet(&bullets[spawned], pos, dir, damage);
      spawned++;
    }
    break;
  }
  case BULLET_PATTERN_STAR: {
    // 星形：6 轴放射（每 60°），每轴带内部微散布
    int perAxis = (count + BULLET_STAR_AXES - 1) / BULLET_STAR_AXES;
    for (int k = 0; k < BULLET_STAR_AXES && spawned < count; k++) {
      float axisBase = 2.0f * PI * (float)k / (float)BULLET_STAR_AXES;
      int thisCount = perAxis;
      if (spawned + thisCount > count)
        thisCount = count - spawned;
      for (int j = 0; j < thisCount; j++) {
        float angle = axisBase + (j - (thisCount - 1) * 0.5f) *
                                     BULLET_STAR_SPREAD / (float)thisCount;
        Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
        InitBullet(&bullets[spawned], origin, vel, damage);
        spawned++;
      }
    }
    break;
  }
  case BULLET_PATTERN_AIMED_WIDE: {
    // 瞄准大扇形：宽角扇形朝向玩家，覆盖更广走位区
    float base = atan2f(target.y - origin.y, target.x - origin.x);
    for (int i = 0; i < count; i++) {
      float angle =
          base + (i - (count - 1) * 0.5f) * BULLET_SPREAD_WIDE / (float)count;
      Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
      InitBullet(&bullets[spawned], origin, vel, damage);
      spawned++;
    }
    break;
  }
  case BULLET_PATTERN_TRIPLE_ARC: {
    // 三段弧：朝玩家左/中/右三组扇形齐射
    float base = atan2f(target.y - origin.y, target.x - origin.x);
    const float centers[3] = {base - BULLET_ARC_GAP, base,
                              base + BULLET_ARC_GAP};
    int each = count / 3;
    int rem = count % 3;
    int groupCounts[3] = {each + (rem > 0 ? 1 : 0), each + (rem > 1 ? 1 : 0),
                          each};
    for (int g = 0; g < 3; g++) {
      for (int j = 0; j < groupCounts[g] && spawned < count; j++) {
        float angle = centers[g] + (j - (groupCounts[g] - 1) * 0.5f) *
                                       BULLET_SPREAD_TIGHT /
                                       (float)groupCounts[g];
        Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
        InitBullet(&bullets[spawned], origin, vel, damage);
        spawned++;
      }
    }
    break;
  }
  case BULLET_PATTERN_HAIL: {
    // 天降：从敌怪处沿屏幕上方多点竖直下落
    for (int i = 0; i < count; i++) {
      float off = (i - (count - 1) * 0.5f) * BULLET_HAIL_SPACING;
      Vector2 pos = {origin.x + off, origin.y};
      Vector2 vel = {0.f, speed};
      InitBullet(&bullets[spawned], pos, vel, damage);
      spawned++;
    }
    break;
  }
  default:
    break;
  }
  return spawned;
}

// 弹幕浮动伤害：返回 [BATTLE_BULLET_DMG_MIN, BATTLE_BULLET_DMG_MAX] 的随机
// 伤害值（默认 3~8，见 core/game_config.h；Hard 波次多，上限 9→8 补偿）
float BulletRollDamage(void) {
  const int range = (int)(BATTLE_BULLET_DMG_MAX - BATTLE_BULLET_DMG_MIN) + 1;
  return BATTLE_BULLET_DMG_MIN + (float)genRandomNum(range);
}
