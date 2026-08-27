#include "game.h"

// boss 尺寸：贴图 32×32 × GAME_SCALE，比普通敌人适当放大
#define BOSS_WIDTH (32.f * GAME_SCALE)
#define BOSS_HEIGHT (32.f * GAME_SCALE)
// boss 移动速度（世界坐标/秒）；boss 不受重力，可自由飞行
#define BOSS_MOVE_SPEED 120.f
// boss 发射弹幕间隔（秒）
#define BOSS_SHOOT_COOLDOWN 1.5f
// boss 弹幕飞行速度（世界坐标/秒）
#define BOSS_BULLET_SPEED 240.f
// boss 弹幕对玩家造成的碰撞伤害
#define BOSS_BULLET_DAMAGE 10.f

// 初始化 boss：加载贴图、初始化动画与冷却计时器
void InitBoss(Boss *boss, Vector2 spawn_pos) {
  if (!boss)
    return;

  boss->hp = 3;

  boss->position = spawn_pos;
  boss->velocity = (Vector2){.x = 0.f, .y = 0.f};
  boss->size = (Vector2){.x = BOSS_WIDTH, .y = BOSS_HEIGHT};

  // boss.png 为 8 帧横排贴图
  boss->bossTexture = LoadTexture(
      TextFormat("%sassets/sprites/boss.png", GetApplicationDirectory()));
  AnimationInit(&boss->animations[BOSS_MOVE], &boss->bossTexture, 8, 0.1f,
                true);

  InitTimer(&boss->cooldownTimer);

  boss->isCoolDown = false;
  boss->isMovable = true;
  boss->isAlive = true;
}

// 每帧更新：推进动画、位移与发射冷却计时。
// boss 不受重力影响（不施加垂直重力加速度），可自由飞行。
void UpdateBoss(Boss *boss, float dt) {
  if (!boss || !boss->isAlive)
    return;

  AnimationUpdate(&boss->animations[BOSS_MOVE], dt);

  // 移动：示例逻辑（恒定向右），后续可替换为往返巡逻 / 追踪玩家等行为
  boss->velocity.x = boss->isMovable ? BOSS_MOVE_SPEED : 0.f;
  boss->position.x += boss->velocity.x * dt;
  boss->position.y += boss->velocity.y * dt;

  // 发射冷却计时：冷却结束前不可发射
  if (boss->isCoolDown) {
    UpdateTimer(&boss->cooldownTimer);
    if (GetElapsedTime(&boss->cooldownTimer) >= BOSS_SHOOT_COOLDOWN) {
      boss->isCoolDown = false;
      ResetTimer(&boss->cooldownTimer);
    }
  }
}

// 绘制 boss：source 为当前动画帧源矩形，绘制尺寸与碰撞 boss->size 一致
void DrawBoss(Boss *boss, Rectangle source) {
  if (!boss || !boss->isAlive)
    return;

  Rectangle dest = {
      .x = boss->position.x,
      .y = boss->position.y,
      .width = boss->size.x,
      .height = boss->size.y,
  };
  DrawTexturePro(boss->bossTexture, source, dest, (Vector2){0.f, 0.f}, 0.f,
                 WHITE);
}

// boss 发射一颗弹幕：初始化传入的 bullet 并进入冷却
void ShootBullet(Boss *boss, Bullet *bullet) {
  if (!boss || !bullet || !boss->isAlive || boss->isCoolDown)
    return;

  // 弹幕出生点：boss 中心；示例：水平朝左发射
  Vector2 spawn = {
      .x = boss->position.x + boss->size.x * 0.5f,
      .y = boss->position.y + boss->size.y * 0.5f,
  };
  Vector2 velocity = {.x = -BOSS_BULLET_SPEED, .y = 0.f};
  InitBullet(bullet, spawn, velocity, BOSS_BULLET_DAMAGE);

  boss->isCoolDown = true;
  ResetTimer(&boss->cooldownTimer);
}
