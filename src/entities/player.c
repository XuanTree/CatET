#include "entities/player.h"
#include "core/game_config.h"
#include "raylib.h"
#include "tools/animation.h"
#include "tools/timer.h"
#include <math.h>
#include <stdbool.h>

#define GRAVITY 980.0f
#define MOVE_SPEED 240.0f
#define JUMP_SPEED (-580.0f)
#define JUMP_HOLD_FORCE 1500.0f // 按住空格期间额外向上加速度
#define JUMP_HOLD_MAX 0.06f     // 最大跳跃增强时长（秒），防止无限升高
#define FRICTION 0.25f
// 玩家帧贴图为 16×16，与全局 GAME_SCALE 统一缩放，保证与平台同比例
#define PLAYER_WIDTH (16.0f * GAME_SCALE)
#define PLAYER_HEIGHT (16.0f * GAME_SCALE)
#define AFK_TIMEOUT 20.0f // 挂机 20 秒后自动进入睡眠动画

void InitPlayer(Player *player) {
  player->health = 5;
  player->maxHealth = 5;

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
  player->idleTexture = LoadTexture(
      TextFormat("%sassets/sprites/cat_idle.png", GetApplicationDirectory()));
  AnimationInit(&player->animations[IDLE], &player->idleTexture, 8, 0.1f, true);
  // 初始化 RUN 动画 （4 帧，每帧 0.1 秒， 循环播放）
  player->runTexture = LoadTexture(
      TextFormat("%sassets/sprites/cat_run.png", GetApplicationDirectory()));
  AnimationInit(&player->animations[WALK], &player->runTexture, 4, 0.1f, true);
  AnimationInit(&player->animations[RUN], &player->runTexture, 4, 0.08f, true);
  // 初始化 jump 动画 （1 帧）
  player->jumpTexture = LoadTexture(
      TextFormat("%sassets/sprites/cat_jump.png", GetApplicationDirectory()));
  AnimationInit(&player->animations[JUMP], &player->jumpTexture, 1, 0.1f,
                false);
  // 初始化 sleep 动画 （4 帧）
  player->sleepTexture = LoadTexture(
      TextFormat("%sassets/sprites/cat_sleep.png", GetApplicationDirectory()));
  AnimationInit(&player->animations[SLEEP], &player->sleepTexture, 4, 0.6f,
                true);
}

void UpdatePlayer(Player *player, float dt) {
  bool jumpPressed =
      IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_SPACE);
  bool jumpHeld = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_SPACE);
  bool jumpReleased =
      IsKeyReleased(KEY_W) || IsKeyReleased(KEY_UP) || IsKeyReleased(KEY_SPACE);

  // 起跳：在地面上且跳跃键按下或按住时立即起跳。
  // 用「按住」判定，保证一直按住空格时落地瞬间能无缝衔接下一次跳跃。
  if (player->isOnTheGround && (jumpPressed || jumpHeld)) {
    player->velocity.y = JUMP_SPEED;
    player->isOnTheGround = false;
    InitTimer(&player->jumpHoldTimer); // 记录起跳时刻
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
  if (!player->isOnTheGround) {
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

void GroundCollision(Player *player) {
  float groundY = 480 - 50; // 地面在 y = 550 处
  if (player->position.y + player->size.y >= groundY) {
    player->position.y = groundY - player->size.y;
    player->velocity.y = 0;
    player->isOnTheGround = true;
  }
}