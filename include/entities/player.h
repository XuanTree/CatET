#ifndef PLAYER_H
#define PLAYER_H

#pragma once
#include "tools/animation.h"
#include "tools/strings.h"
#include "tools/timer.h"
#include <raylib.h>

typedef enum PlayerAnimation {
  IDLE,
  WALK,
  RUN,
  JUMP,
  SLEEP,
  COUNT
} PlayerAnimation;

typedef struct Player {
  int health;
  int maxHealth;
  Vector2 position;
  Vector2 velocity;
  Texture2D idleTexture;
  Texture2D runTexture;
  Texture2D jumpTexture;
  Texture2D sleepTexture;
  Vector2 size;
  Animation animations[COUNT];
  PlayerAnimation playerAnimationState;
  Timer jumpHoldTimer;
  Timer afkTimer;
  bool isOnTheGround;
  bool facingRight; // 面朝方向
} Player;

void InitPlayer(Player *player);
void UpdatePlayer(Player *player, float dt);
void DrawPlayer(Player *player, Rectangle source);
void GroundCollision(Player *player);

#endif // PLAYER_H
