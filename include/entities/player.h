#ifndef PLAYER_H
#define PLAYER_H

#pragma once
#include "core/gameapp.h"
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
  const GameApp *app; // 音频宿主引用（由场景在 InitPlayer 后注入，用于播放
                      // cat_hit/cat_jump 音效；NULL 时静默跳过）
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
  float hitTimer;   // 受伤动画剩余时长（秒，>0 表示处于受伤状态）
  float lastHealth; // 上一帧生命值（用于检测生命值下降以触发受伤动画）
} Player;

void InitPlayer(Player *player);
void UpdatePlayer(Player *player, float dt);
void DrawPlayer(Player *player, Rectangle source);
void GroundCollision(Player *player);

#endif // PLAYER_H
