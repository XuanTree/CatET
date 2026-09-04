#include "game.h"

#define GRAVITY 980.0f
#define MOVE_SPEED 240.0f
#define JUMP_SPEED (-580.0f)
#define JUMP_HOLD_FORCE 1500.0f // 按住空格期间额外向上加速度
#define JUMP_HOLD_MAX 0.06f     // 最大跳跃增强时长（秒），防止无限升高
#define FRICTION 0.25f
// 玩家帧贴图为 16×16，与全局 GAME_SCALE 统一缩放，保证与平台同比例
#define PLAYER_WIDTH (16.0f * GAME_SCALE)
#define PLAYER_HEIGHT (16.0f * GAME_SCALE)
#define AFK_TIMEOUT 20.0f         // 挂机 20 秒后自动进入睡眠动画
#define HIT_DURATION 0.8f         // 受伤动画总时长（秒，强制播放 0.8s）
#define HIT_KNOCKBACK_UP (-260.f) // 受伤时强制轻微上跳（增强视觉）

void InitPlayer(Player *player) {
  player->health = 100.f;
  player->maxHealth = 100.f;

  player->position = (Vector2){.x = 100, .y = 300};
  player->velocity = (Vector2){.x = 0, .y = 0};
  player->size = (Vector2){.x = PLAYER_WIDTH, .y = PLAYER_HEIGHT};

  player->isOnTheGround = false;
  player->playerAnimationState = IDLE;
  player->facingRight = true;
  InitTimer(&player->jumpHoldTimer); // 初始化跳跃增强计时器（防止未初始化读取）
  InitTimer(&player->afkTimer);      // 初始化挂机计时器
  // 资源统一从 exe 同级的 assets/ 目录加载（CMake POST_BUILD 自动复制）

  // 初始化 IDLE 动画（8 帧，每帧 0.1 秒，循环播放）
  player->idleTexture = LoadEmbeddedTexture("assets/sprites/cat_idle.png");
  AnimationInit(&player->animations[IDLE], &player->idleTexture, 8, 0.1f, true);
  // 初始化 RUN 动画 （4 帧，每帧 0.1 秒， 循环播放）
  player->runTexture = LoadEmbeddedTexture("assets/sprites/cat_run.png");
  AnimationInit(&player->animations[WALK], &player->runTexture, 4, 0.1f, true);
  AnimationInit(&player->animations[RUN], &player->runTexture, 4, 0.08f, true);
  // 初始化 jump 动画 （1 帧）
  player->jumpTexture = LoadEmbeddedTexture("assets/sprites/cat_jump.png");
  AnimationInit(&player->animations[JUMP], &player->jumpTexture, 1, 0.1f,
                false);
  // 初始化 sleep 动画 （4 帧）
  player->sleepTexture = LoadEmbeddedTexture("assets/sprites/cat_sleep.png");
  AnimationInit(&player->animations[SLEEP], &player->sleepTexture, 4, 0.6f,
                true);

  // 初始化 HIT 动画（cat_hit.png 为 16×16×4 帧横排，不循环，总时长 0.8s）
  player->hitTexture = LoadEmbeddedTexture("assets/sprites/cat_hit.png");
  AnimationInit(&player->animations[HIT], &player->hitTexture, 4,
                HIT_DURATION / 4.f, false);
  player->hitTimer = 0.f;
  player->lastHealth = player->health;
}

void UpdatePlayer(Player *player, float dt) {
  bool jumpPressed =
      IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_SPACE);
  bool jumpHeld = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_SPACE);
  bool jumpReleased =
      IsKeyReleased(KEY_W) || IsKeyReleased(KEY_UP) || IsKeyReleased(KEY_SPACE);

  // 受伤检测：生命值下降时强制播放 0.8s 受伤动画，并轻微上跳增强视觉
  if (player->health < player->lastHealth) {
    player->hitTimer = HIT_DURATION;
    player->velocity.y = HIT_KNOCKBACK_UP;
    player->isOnTheGround = false;
    // 受伤音效（cat_hit.ogg）：生命值下降瞬间播放一次
    GameAppPlaySound(player->app, player->app->catHitSound,
                     player->app->catHitSoundValid);
  }
  player->lastHealth = player->health;

  // 起跳：在地面上且跳跃键按下或按住时立即起跳。
  // 用「按住」判定，保证一直按住空格时落地瞬间能无缝衔接下一次跳跃。
  if (player->isOnTheGround && (jumpPressed || jumpHeld)) {
    player->velocity.y = JUMP_SPEED;
    player->isOnTheGround = false;
    InitTimer(&player->jumpHoldTimer); // 记录起跳时刻
    // 跳跃音效（cat_jump.ogg）：每次起跳瞬间播放一次
    GameAppPlaySound(player->app, player->app->catJumpSound,
                     player->app->catJumpSoundValid);
  }

  // 按住期间：持续累加蓄力时间，并在上升阶段持续提供向上力
  if (jumpHeld) {
    UpdateTimer(&player->jumpHoldTimer);
    float holdDuration = GetElapsedTime(&player->jumpHoldTimer);
    // 还在上升（velocity.y < 0）且未超过最大增强时长时，持续施加额外向上加速度
    if (!player->isOnTheGround && player->velocity.y < 0 &&
        holdDuration < JUMP_HOLD_MAX) {
      player->velocity.y -= JUMP_HOLD_FORCE * dt;
    }
  }

  // 松开瞬间：若还在上升，截断上升速度（点按=小跳，长按=高跳）
  if (jumpReleased && player->velocity.y < 0) {
    player->velocity.y *= 0.5f;
    ResetTimer(&player->jumpHoldTimer);
  }

  // 重力
  player->velocity.y += GRAVITY * dt;
  player->position.y += player->velocity.y * dt;

  // 水平输入
  if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
    if (IsKeyDown(KEY_LEFT_SHIFT)) {
      player->velocity.x = -MOVE_SPEED * 1.6f;
    } else {
      player->velocity.x = -MOVE_SPEED;
    }
    player->facingRight = false;
  } else if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
    if (IsKeyDown(KEY_LEFT_SHIFT)) {
      player->velocity.x = MOVE_SPEED * 1.6f;
    } else {
      player->velocity.x = MOVE_SPEED;
    }
    player->facingRight = true;
  } else {
    player->velocity.x *= FRICTION;
    if (fabsf(player->velocity.x) < 1.0f) {
      player->velocity.x = 0.f;
    }
  }

  // 挂机检测：只要有任何输入（跳跃或移动）就重置计时器，否则累计挂机时间
  bool hasInput = jumpPressed || jumpHeld || IsKeyDown(KEY_A) ||
                  IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_D) ||
                  IsKeyDown(KEY_RIGHT);
  if (hasInput) {
    ResetTimer(&player->afkTimer);
  } else {
    UpdateTimer(&player->afkTimer);
  }
  float afkTime = GetElapsedTime(&player->afkTimer);

  // 根据玩家不同的状态绘制不同的动画
  // 受伤状态优先：强制播放受伤动画（不循环），0.8s 后自动恢复
  if (player->hitTimer > 0.f) {
    player->hitTimer -= dt;
    if (player->hitTimer <= 0.f)
      player->hitTimer = 0.f;
    player->playerAnimationState = HIT;
  } else if (!player->isOnTheGround) {
    player->playerAnimationState = JUMP;
  } else if (fabsf(player->velocity.x) > MOVE_SPEED) {
    player->playerAnimationState = RUN;
  } else if (fabsf(player->velocity.x) > 0.1f) {
    player->playerAnimationState = WALK;
  } else if (afkTime >= AFK_TIMEOUT) {
    player->playerAnimationState = SLEEP;
  } else {
    player->playerAnimationState = IDLE;
  }

  // 水平移动
  player->position.x += player->velocity.x * dt;
}

void DrawPlayer(Player *player, Rectangle source) {
  Rectangle src = source;
  if (!player->facingRight) {
    // 源矩形水平翻转：x 移到帧右边缘，宽度取负
    src.x = source.x + source.width;
    src.width = -source.width;
  }
  // 绘制尺寸与碰撞 player->size 严格一致，避免精灵缩放与碰撞盒错位
  Rectangle dest = {
      .x = player->position.x,
      .y = player->position.y,
      .width = player->size.x,
      .height = player->size.y,
  };
  // 根据玩家动画状态选择对应的纹理绘制
  switch (player->playerAnimationState) {
  case JUMP:
    DrawTexturePro(player->jumpTexture, src, dest, (Vector2){0, 0}, 0.0f,
                   WHITE);
    break;
  case HIT:
    DrawTexturePro(player->hitTexture, src, dest, (Vector2){0, 0}, 0.0f, WHITE);
    break;
  case WALK:
  case RUN:
    DrawTexturePro(player->runTexture, src, dest, (Vector2){0, 0}, 0.0f, WHITE);
    break;
  case SLEEP:
    DrawTexturePro(player->sleepTexture, src, dest, (Vector2){0, 0}, 0.0f,
                   WHITE);
    break;
  case IDLE:
  default:
    DrawTexturePro(player->idleTexture, src, dest, (Vector2){0, 0}, 0.0f,
                   WHITE);
    break;
  }
}

// 玩家命中/受击盒（世界坐标）：精灵帧 16×16 内主体约占中央 x[1..14]、
// y[2..14]，四周为透明/轮廓留白。返回值相对 player->size 整幅收窄：
//   左/右各内缩 1px、顶部内缩 2px、底部内缩 1px（×GAME_SCALE 换算为世界），
// 结果矩形与整盒同锚点方向（左上角右移 3、下移 6，宽 42、高 39）。
Rectangle PlayerHitRect(const Player *player) {
  if (!player)
    return (Rectangle){0, 0, 0, 0};
  const float insetLeft = 1.0f * GAME_SCALE;
  const float insetTop = 2.0f * GAME_SCALE;
  const float insetRight = 1.0f * GAME_SCALE;
  const float insetBottom = 1.0f * GAME_SCALE;
  return (Rectangle){
      .x = player->position.x + insetLeft,
      .y = player->position.y + insetTop,
      .width = player->size.x - insetLeft - insetRight,
      .height = player->size.y - insetTop - insetBottom,
  };
}

// 纯矩形地面碰撞：地面视为一个静态矩形（顶面位于 y = LOGIC_HEIGHT - 50 =
// 430，高度固定 50）。用玩家矩形与地面矩形做 AABB 重叠检测
// （CheckCollisionRecs），重叠时把玩家推出地面：
//   - 从上方向下落到地面顶面 → 站在地面上（isOnTheGround = true）
//   - 从地面下方顶头（防御性处理）→ 阻止穿入
// groundWidth 由调用场景传入，须与各场景绘制的地面宽度一致：例如平台关卡
// 绘制宽 = logicWidth，测试关卡绘制宽 = 1000。此前硬编码 1000 导致平台关卡
// 碰撞面比可视地面宽出 (1000 - logicWidth)，玩家向右越过可视地面边缘仍不会
// 掉落（视觉与碰撞错位）。
void GroundCollision(Player *player, float groundWidth) {
  if (!player)
    return;

  // 地面矩形：宽度取调用方传入值（非法值兜底 1000，与旧行为一致）
  const float groundTop = 480.0f - 50.0f;
  const float width = (groundWidth > 0.f) ? groundWidth : 1000.0f;
  const Rectangle ground = {
      .x = 0, .y = groundTop, .width = width, .height = 50.0f};
  const Rectangle playerRect = {
      .x = player->position.x,
      .y = player->position.y,
      .width = player->size.x,
      .height = player->size.y,
  };

  // 纯矩形重叠检测：玩家矩形与地面矩形无重叠则不碰撞
  if (!CheckCollisionRecs(playerRect, ground))
    return;

  // 重叠后按相对位置 / 速度方向解决碰撞
  if (player->velocity.y >= 0.0f &&
      player->position.y + player->size.y >= groundTop) {
    // 下落且脚底已触及地面顶面 → 落地
    player->position.y = groundTop - player->size.y;
    player->velocity.y = 0.0f;
    player->isOnTheGround = true;
  } else if (player->velocity.y < 0.0f && player->position.y <= groundTop) {
    // 上升且头顶已触及地面 → 顶头（防御：阻止穿出地面底部）
    player->position.y = groundTop;
    player->velocity.y = 0.0f;
  }
}

// 通关奖励：恢复固定生命值（封顶到最大生命值，避免溢出）
void PlayerHeal(Player *player, float amount) {
  if (!player || amount <= 0.0f)
    return;
  player->health += amount;
  if (player->health > player->maxHealth)
    player->health = player->maxHealth;
}

// 按难度应用最大生命值（Easy/Normal=100，Hard=125）。仅改 maxHealth，
// 当前 health 由场景随后按「继承 or 新游戏满血」逻辑赋值。
void PlayerApplyDifficulty(Player *player, int difficulty) {
  if (!player)
    return;
  const float base = PLAYER_MAX_HEALTH_BASE;
  player->maxHealth =
      (difficulty >= 2) ? base * PLAYER_MAX_HEALTH_HARD_MULT : base;
  if (player->health > player->maxHealth)
    player->health = player->maxHealth;
}

// 触发玩家受伤动画：强制播放 HIT（时长与 UpdatePlayer 内一致）
void PlayerTriggerHit(Player *player) {
  if (!player)
    return;
  player->hitTimer = HIT_DURATION;
  player->playerAnimationState = HIT;
}