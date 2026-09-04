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
// 玩家命中/受击盒（世界坐标矩形）：比 player->size 整幅略小、居中于精灵主体
// （玩家帧贴图 16×16 内主体约占 x[1..14]、y[2..14]，四周为透明/轮廓留白）。
// 弹幕等“精确命中”判定应使用本接口（配合 BulletHitCircle 做圆-矩形检测），
// 避免弹幕仅贴到透明边缘/轮廓就被判定命中、造成“还没碰到就受伤”的观感。
// 平台/地面/敌怪/旗子等仍用 player->size 整盒，保证落地与触碰判定不悬浮。
Rectangle PlayerHitRect(const Player *player);
// 纯矩形地面碰撞：groundWidth 为地面宽度（调用场景须传入与自身绘制一致的
// 宽度，例如平台关卡传 logicWidth、测试关卡传
// 1000，避免碰撞面与可视地面错位）。
void GroundCollision(Player *player, float groundWidth);

// 通关奖励：恢复固定生命值（封顶到最大生命值，避免溢出）。
void PlayerHeal(Player *player, float amount);

// 按难度应用最大生命值（Easy/Normal=100，Hard=125，见 game_config 的
// PLAYER_MAX_HEALTH_BASE / PLAYER_MAX_HEALTH_HARD_MULT）。须在场景 Enter 的
// InitPlayer 之后、生命值继承/新游戏满血赋值之前调用（仅改 maxHealth，
// 不改变当前 health）。
void PlayerApplyDifficulty(Player *player, int difficulty);

// 触发玩家受伤动画：强制播放 HIT（不循环），时长与 UpdatePlayer 内一致。
// 用于战斗等直接扣血场景（若只同步 lastHealth 而不触发，玩家受伤将无
// HIT 动画表现）。
void PlayerTriggerHit(Player *player);

#endif // PLAYER_H
