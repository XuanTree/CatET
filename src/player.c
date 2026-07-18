#include "player.h"
#include "raylib.h"
#include <math.h>
#include <stdbool.h>
#include <xkeycheck.h>

#define GRAVITY 980.0f
#define MOVE_SPEED 200.0f
#define JUMP_SPEED -400.0f
#define FRICTION 0.85f
#define PLAYER_WIDTH 48.0f
#define PLAYER_HEIGHT 48.0f

void InitPlayer(Player *player, String *spriteFilePath) {
  player->health = 5;
  player->maxHealth = 5;

  player->position = (Vector2){100, 300};
  player->velocity = (Vector2){0, 0};
  player->size = (Vector2){PLAYER_WIDTH, PLAYER_HEIGHT};

  player->isOnTheGround = false;
  player->playerAnimationState = IDLE;
}

void UpdatePlayer(Player *player, float dt) {
  // GRAVITY
  player->velocity.y += GRAVITY * dt;
  player->position.y += player->velocity.y * dt;

  // Get Input from user
  if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
    player->velocity.x = -MOVE_SPEED;
  } else if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
    player->velocity.x = MOVE_SPEED;
  } else {
    player->velocity.x *= FRICTION; // fraction;
    if (fabsf(player->velocity.x) < 1.0f) {
      player->velocity.x = 0.f;
    }
  } // Jump!
  if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_SPACE)) {
    if (player->isOnTheGround) {
      player->velocity.y = JUMP_SPEED;
      player->isOnTheGround = false;
    }
  }

  // Move and Slide
  player->position.x += player->velocity.x * dt;
}

void DrawPlayer(Player *player) {}