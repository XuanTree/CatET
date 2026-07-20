#include "player.h"
#include "raylib.h"
#include <math.h>
#include <stdbool.h>

#define GRAVITY 980.0f
#define MOVE_SPEED 240.0f
#define JUMP_SPEED (-580.0f)
#define FRICTION 0.85f
#define PLAYER_WIDTH 48.0f
#define PLAYER_HEIGHT 48.0f

void InitPlayer(Player *player) {
  player->health = 5;
  player->maxHealth = 5;

  player->position = (Vector2){.x = 100, .y = 300};
  player->velocity = (Vector2){.x = 0, .y = 0};
  player->size = (Vector2){.x = PLAYER_WIDTH, .y = PLAYER_HEIGHT};

  player->isOnTheGround = false;
  player->playerAnimationState = IDLE;
  player->facingRight = true;
  // 从 exe 目录上溯 3 级到项目根目录，加载 spritesheet
  // WHY!!!! 这BUG必须修了...? BUG
  player->playerTexture = LoadTexture(TextFormat(
      "%s../../../assets/sprites/CatIdle.png", GetApplicationDirectory()));

  // 初始化 IDLE 动画（8 帧，每帧 0.1 秒，循环播放）
  AnimationInit(&player->animations[IDLE], &player->playerTexture, 8, 0.1f,
                true);
}

void UpdatePlayer(Player *player, float dt) {
  // 先处理跳跃，防止玩家在地面上跳不起来
  if (player->isOnTheGround &&
      ((IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP) ||
        IsKeyPressed(KEY_SPACE)) ||
       (IsKeyDown(KEY_W) || IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_UP)))) {
    player->velocity.y = JUMP_SPEED;
    player->isOnTheGround = false;
  }

  // 重力
  player->velocity.y += GRAVITY * dt;
  player->position.y += player->velocity.y * dt;

  // 水平输入
  if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
    player->velocity.x = -MOVE_SPEED;
    player->facingRight = false;
  } else if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
    player->velocity.x = MOVE_SPEED;
    player->facingRight = true;
  } else {
    player->velocity.x *= FRICTION;
    if (fabsf(player->velocity.x) < 1.0f) {
      player->velocity.x = 0.f;
    }
  }

  // 水平移动
  player->position.x += player->velocity.x * dt;
}

void DrawPlayer(Player *player, Rectangle source) {
  float scale = 3.f;
  Rectangle src = source;
  if (!player->facingRight) {
    // 源矩形水平翻转：x 移到帧右边缘，宽度取负
    src.x = source.x + source.width;
    src.width = -source.width;
  }
  Rectangle dest = {
      .x = player->position.x,
      .y = player->position.y,
      .width = source.width * scale,
      .height = source.height * scale,
  };
  DrawTexturePro(player->playerTexture, src, dest, (Vector2){0, 0}, 0.0f,
                 WHITE);
}

void GroundCollision(Player *player) {
  float groundY = 600 - 50; // 地面在 y = 550 处
  if (player->position.y + player->size.y >= groundY) {
    player->position.y = groundY - player->size.y;
    player->velocity.y = 0;
    player->isOnTheGround = true;
  }
}