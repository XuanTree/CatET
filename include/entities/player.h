/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

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
  bool facingRight;      // 面朝方向
  bool isMovable;        // 规定玩家是否可以移动，在战斗场景中，玩家不允许移动
  float hitTimer;        // 受伤动画剩余时长（秒，>0 表示处于受伤状态）
  float lastHealth;      // 上一帧生命值（用于检测生命值下降以触发受伤动画）
  float invincibleTimer; // 无敌时间剩余（秒，>0 期间不受伤害且绘制时闪烁表现，
                         // 用于战斗弹幕命中后给予短暂免伤，避免连续扣血）
} Player;

void InitPlayer(Player *player);
void UpdatePlayer(Player *player, float dt);
void DrawPlayer(Player *player, Rectangle source);
void GroundCollision(Player *player);

// 通关奖励：恢复固定生命值（封顶到最大生命值，避免溢出）。
void PlayerHeal(Player *player, float amount);

// 触发玩家受伤动画：强制播放 HIT（不循环），时长与 UpdatePlayer 内一致。
// 用于战斗等直接扣血场景（若只同步 lastHealth 而不触发，玩家受伤将无
// HIT 动画表现）。
void PlayerTriggerHit(Player *player);

#endif // PLAYER_H
