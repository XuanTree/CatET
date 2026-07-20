#ifndef PLAYER_H
#define PLAYER_H

#pragma once
#include "animation.h"
#include "core/strings.h"
#include <raylib.h>

typedef enum PlayerAnimation { IDLE, RUN, JUMP, COUNT } PlayerAnimation;

typedef struct Player {
  int health;
  int maxHealth;
  Vector2 position;
  Vector2 velocity;
  Texture2D playerTexture;
  Vector2 size;
  Animation animations[COUNT];
  PlayerAnimation playerAnimationState;
  bool isOnTheGround;
  bool facingRight; // 面朝方向
} Player;

void InitPlayer(Player *player);
void UpdatePlayer(Player *player, float dt);
void DrawPlayer(Player *player, Rectangle source);
void GroundCollision(Player *player);

#endif // PLAYER_H