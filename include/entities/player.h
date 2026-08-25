#ifndef PLAYER_H
#define PLAYER_H

#pragma once
#include "tools/animation.h"
#include "tools/timer.h"
#include <raylib.h>

typedef enum PlayerAnimation {
  IDLE,
  WALK,
  RUN,
  JUMP,
  HIT,
  SLEEP,
  COUNT
} PlayerAnimation;

typedef struct Player {
  float health;
  float maxHealth;
  Vector2 position;
  Vector2 velocity;
  Texture2D idleTexture;
  Texture2D runTexture;
  Texture2D jumpTexture;
  Texture2D sleepTexture;
  Texture2D hitTexture;
  Vector2 size;
  Animation animations[COUNT];
  PlayerAnimation playerAnimationState;
  Timer jumpHoldTimer;
  Timer afkTimer;
  bool isOnTheGround;
  bool facingRight; // 面朝方向
  bool isMovable;   // 规定玩家是否可以移动，在战斗场景中，玩家不允许移动
} Player;

void InitPlayer(Player *player);
void UpdatePlayer(Player *player, float dt);
void DrawPlayer(Player *player, Rectangle source);
void GroundCollision(Player *player);

#endif // PLAYER_H
