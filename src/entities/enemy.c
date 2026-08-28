#include "game.h"

// 敌怪移动速度（世界坐标/秒）
#define ENEMY_MOVE_SPEED 260.f
// 敌怪帧贴图为 16×16，与全局 GAME_SCALE 统一缩放，与玩家/平台同比例
#define ENEMY_WIDTH (16.f * GAME_SCALE)
#define ENEMY_HEIGHT (16.f * GAME_SCALE)
// 重力（与玩家 GRAVITY 一致，保持同一世界的物理手感）
#define ENEMY_GRAVITY 980.f
// 巡逻参数：单程行走 / 原地停顿时长
#define ENEMY_WALK_TIME 1.8f
#define ENEMY_AFK_TIME 1.2f
// 战斗：触碰后的 1s 定格窗口时长、触发后的冷却时长
#define ENEMY_BATTLE_DELAY 1.0f
#define ENEMY_BATTLE_COOLDOWN 2.0f
// 地面矩形（与 scene_test 绘制 / 玩家 GroundCollision 一致）：顶面 y = 480-50
#define ENEMY_GROUND_TOP (480.f - 50.f)
#define ENEMY_GROUND_WIDTH 1000.f
#define ENEMY_GROUND_HEIGHT 50.f

// 敌人世界坐标矩形（绘制与碰撞统一）
static Rectangle EnemyRect(const Enemy *enemy) {
  return (Rectangle){enemy->position.x, enemy->position.y, enemy->size.x,
                     enemy->size.y};
}

// 玩家世界坐标矩形（与碰撞一致）
static Rectangle PlayerRect(const Player *player) {
  return (Rectangle){player->position.x, player->position.y, player->size.x,
                     player->size.y};
}

// 初始化敌人数据，默认敌人可以移动，敌怪受重力影响
void InitEnemy(Enemy *enemy, Vector2 spawn_pos) {
  enemy->hp = 3;

  enemy->position = spawn_pos;
  enemy->velocity = (Vector2){.x = 0, .y = 0};
  enemy->size = (Vector2){.x = ENEMY_WIDTH, .y = ENEMY_HEIGHT};

  // 初始化敌人内部自带的计时器（行走 / 停顿 / 战斗）
  InitTimer(&enemy->walkingTimer);
  InitTimer(&enemy->afkTimer);
  InitTimer(&enemy->battleTimer);

  // 初始化 IDLE 动画（4 帧，每帧 0.1s，循环播放）
  enemy->idleTexture = LoadEmbeddedTexture("assets/sprites/paper_enemy.png");
  AnimationInit(&enemy->animations[ENEMY_MOVE], &enemy->idleTexture, 4, 0.1f,
                true);

  // 巡逻初始处于行走阶段（先向右走）
  enemy->isWalking = true;
  enemy->isHaveGoneRight = true;
  enemy->isOnTheGround = false;
  enemy->isMovable = true;
  enemy->isAlive = true;
  enemy->isCountdown = false;
  enemy->isCooldown = false;

  // 战斗回调默认空，由场景注入
  enemy->onBattle = NULL;
  enemy->battleCtx = NULL;
}

// 敌人巡逻状态机 + 重力 + 位移。
// 巡逻：单程行走 ENEMY_WALK_TIME（1.8s）→ 原地停顿 ENEMY_AFK_TIME（1.2s）
// → 反向行走，反复（对齐注释“走路时间 1.8s，原地停顿 1.2s”）。
// 用两个计时器分别统计行走/停顿时长（ResetTimer 归零开始时间），
// 不依赖逐帧累积，暂停/切帧也不漂移。
void UpdateEnemy(Enemy *enemy, float dt) {
  if (!enemy || !enemy->isAlive)
    return;

  if (enemy->isMovable) {
    if (enemy->isWalking) {
      UpdateTimer(&enemy->walkingTimer);
      // 行走：按当前方向移动（isHaveGoneRight 兼作面朝方向）
      enemy->velocity.x =
          enemy->isHaveGoneRight ? ENEMY_MOVE_SPEED : -ENEMY_MOVE_SPEED;
      if (GetElapsedTime(&enemy->walkingTimer) >= ENEMY_WALK_TIME) {
        // 单程行走结束 → 原地停顿
        enemy->isWalking = false;
        enemy->velocity.x = 0.f;
        ResetTimer(&enemy->walkingTimer);
        ResetTimer(&enemy->afkTimer);
      }
    } else {
      UpdateTimer(&enemy->afkTimer);
      if (GetElapsedTime(&enemy->afkTimer) >= ENEMY_AFK_TIME) {
        // 停顿结束 → 反向，重新开始行走
        enemy->isWalking = true;
        enemy->isHaveGoneRight = !enemy->isHaveGoneRight;
        ResetTimer(&enemy->walkingTimer);
        ResetTimer(&enemy->afkTimer);
      }
    }
  } else {
    // 不可移动（如战斗场景）：横向速度清零，仅受重力
    enemy->velocity.x = 0.f;
  }

  // 重力与位移
  enemy->velocity.y += ENEMY_GRAVITY * dt;
  enemy->position.x += enemy->velocity.x * dt;
  enemy->position.y += enemy->velocity.y * dt;
}

void DrawEnemy(Enemy *enemy, Rectangle source) {
  if (!enemy || !enemy->isAlive)
    return;

  Rectangle src = source;
  if (!enemy->isHaveGoneRight) {
    // 源矩形水平翻转：x 移到帧右边缘，宽度取负
    src.x = source.x + source.width;
    src.width = -source.width;
  }
  // 绘制尺寸与碰撞 enemy->size 严格一致，避免精灵缩放与碰撞盒错位
  Rectangle dest = {
      .x = enemy->position.x,
      .y = enemy->position.y,
      .width = enemy->size.x,
      .height = enemy->size.y,
  };
  DrawTexturePro(enemy->idleTexture, src, dest, (Vector2){0, 0}, 0.f, WHITE);
}

// 敌怪与玩家的碰撞：触碰即进入 1s 定格窗口（画面由场景冻结），满 1s 调用
// onBattle 进入战斗场景；玩家触碰敌怪不扣血。
// 设计要点：
//   - 触碰瞬间（矩形重叠上升沿）置 isCountdown，开始 1s 定格；
//   - 定格期间无论玩家是否仍在接触，满 1s 后必然进入战斗（不可逃离）；
//   - 触发后进入 isCooldown 冷却，避免占位模式下玩家仍贴着敌怪立即再次定格。
void ePlayerCollision(Enemy *enemy, Player *player) {
  if (!enemy || !player || !enemy->isAlive)
    return;

  // 战斗冷却中：不检测碰撞，防止占位模式下触发后立即再次定格形成死循环
  if (enemy->isCooldown) {
    UpdateTimer(&enemy->battleTimer);
    if (GetElapsedTime(&enemy->battleTimer) >= ENEMY_BATTLE_COOLDOWN)
      enemy->isCooldown = false;
    return;
  }

  // 未进入定格窗口：矩形重叠的上升沿 → 置 isCountdown
  if (!enemy->isCountdown) {
    if (!CheckCollisionRecs(EnemyRect(enemy), PlayerRect(player)))
      return;
    enemy->isCountdown = true;
    ResetTimer(&enemy->battleTimer);
    // TODO: 音效资源就绪后，在此（触碰瞬间）播放触碰提醒音效
    return;
  }

  // 定格窗口：持续推进计时，满 1s 触发战斗回调
  UpdateTimer(&enemy->battleTimer);
  if (GetElapsedTime(&enemy->battleTimer) >= ENEMY_BATTLE_DELAY) {
    enemy->isCountdown = false;
    enemy->isCooldown = true;
    ResetTimer(&enemy->battleTimer);
    if (enemy->onBattle)
      enemy->onBattle(enemy->battleCtx);
  }
}

// 敌怪与纯矩形地面的碰撞：与玩家 GroundCollision 使用同一地面矩形
// （顶面 y = 480 - 50 = 430，宽 1000），重叠时按相对位置 / 速度方向解决：
//   - 从上方下落到地面顶面 → 落地（isOnTheGround = true）
//   - 从地面下方顶头（防御性处理）→ 阻止穿入
void eGroundCollision(Enemy *enemy) {
  if (!enemy || !enemy->isAlive)
    return;
  const Rectangle ground = {.x = 0,
                            .y = ENEMY_GROUND_TOP,
                            .width = ENEMY_GROUND_WIDTH,
                            .height = ENEMY_GROUND_HEIGHT};
  const Rectangle enemyRect = EnemyRect(enemy);
  if (!CheckCollisionRecs(enemyRect, ground))
    return;

  if (enemy->velocity.y >= 0.f &&
      enemy->position.y + enemy->size.y >= ENEMY_GROUND_TOP) {
    // 下落且脚底已触及地面顶面 → 落地
    enemy->position.y = ENEMY_GROUND_TOP - enemy->size.y;
    enemy->velocity.y = 0.f;
    enemy->isOnTheGround = true;
  } else if (enemy->velocity.y < 0.f && enemy->position.y <= ENEMY_GROUND_TOP) {
    // 上升且头顶已触及地面 → 顶头（防御：阻止穿出地面底部）
    enemy->position.y = ENEMY_GROUND_TOP;
    enemy->velocity.y = 0.f;
  }
}

// 敌怪与单向平台的碰撞：语义与 PlayerCollision 一致——
// 只提供「上方支撑」，从下方/侧面穿过一律不响应，从上方落到顶面可站立。
void ePlatformCollision(Enemy *enemy, Platform *platform) {
  if (!enemy || !platform || !enemy->isAlive)
    return;
  // 仅当贴图已加载（尺寸有效）时参与碰撞
  if (platform->size.x <= 0.f || platform->size.y <= 0.f)
    return;

  // 平台碰撞矩形（size/surfaceOffset 已按 GAME_SCALE 换算为世界坐标）：
  // 顶面从可见表面起算，忽略贴图顶部透明留白，避免敌怪脚悬空。
  const float platTop = platform->spawnPosition.y + platform->surfaceOffset;
  const Rectangle platformRect = {
      .x = platform->spawnPosition.x,
      .y = platTop,
      .width = platform->size.x,
      .height = platform->size.y - platform->surfaceOffset,
  };
  const Rectangle enemyRect = EnemyRect(enemy);
  if (!CheckCollisionRecs(enemyRect, platformRect))
    return;

  // 单向平台语义：只处理「下落且重心仍在平台顶面上方」→ 从上方落到平台。
  const float enemyCenterY = enemy->position.y + enemy->size.y * 0.5f;
  if (enemy->velocity.y >= 0.f && enemyCenterY < platTop) {
    enemy->position.y = platTop - enemy->size.y;
    enemy->velocity.y = 0.f;
    enemy->isOnTheGround = true;
  }
}
