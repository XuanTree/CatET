#include "player.h"
#include "raylib.h"
#include <math.h>
#include <stdbool.h>

#define GRAVITY 980.0f
#define MOVE_SPEED 200.0f
#define JUMP_SPEED -400.0f
#define FRICTION 0.85f

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