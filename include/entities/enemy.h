/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef ENEMY_H
#define ENEMY_H

#pragma once
#include "core/game_config.h"
#include "entities/platform.h"
#include "entities/player.h"
#include "tools/animation.h"
#include "tools/timer.h"
#include <raylib.h>
#include <stdbool.h>

typedef enum EnemyAnimation { ENEMY_MOVE, ENEMY_COUNT } EnemyAnimation;

// 战斗事件回调：敌怪与玩家碰撞并经过 1s 定格窗口后触发，由场景注入
// （例如 GameStackPush 到战斗场景）；战斗场景未实现时场景注入占位实现。
typedef void (*EnemyBattleCallback)(void *ctx);

typedef struct Enemy {
  // 生命（=战斗胜利所需选对次数，见战斗场景设计；玩家触碰敌怪不扣血）
  int hp;
  // 敌怪目前只有移动（行走）动画贴图：paper_enemy.png 为 16×16×4 帧横排
  Texture2D idleTexture;
  Animation animations[ENEMY_COUNT];

  // 移动 / 物理
  Vector2 position; // 世界坐标左上角
  Vector2 velocity; // 世界坐标速度
  Vector2 size;     // 世界坐标尺寸（贴图尺寸 × GAME_SCALE，绘制与碰撞共用）

  // 敌人内部自带的计时器：行走 / 停顿 / 战斗
  Timer walkingTimer; // 单程行走计时（1.8s）
  Timer afkTimer;     // 行走间原地停顿时长计时（1.2s）
  Timer battleTimer;  // 战斗计时：1s 定格窗口 / 触发后冷却（复用）

  // 战斗回调：1s 定格结束触发（场景注入），battleCtx 为传给回调的上下文
  EnemyBattleCallback onBattle;
  void *battleCtx;

  // 巡逻状态机
  bool isWalking;       // 当前是否处于行走阶段（false 为停顿时段）
  bool isHaveGoneRight; // 上次行走是否朝右（下次取反，兼作面朝方向）

  // 状态标记
  bool isOnTheGround; // 是否站在地面/平台上
  bool isMovable;     // 是否允许巡逻移动（战斗场景中置 false 静止）
  bool isAlive;       // 战斗胜利后置 false（“删除该敌怪”）
  bool isCountdown;   // 定格窗口：触碰后 1s 内为 true，场景据此冻结画面
  bool isCooldown;    // 触发战斗后的冷却，防止占位模式下立即再次定格（死循环）
} Enemy;

// 初始化敌人数据，默认敌人可以移动
void InitEnemy(Enemy *enemy, Vector2 spawn_pos);

// 每帧更新：巡逻往返移动（单程 1.8s + 原地停顿 1.2s）、重力与位移。
// 不推进动画（动画由场景调用 AnimationUpdate 并与 DrawEnemy 的 source 配合）。
void UpdateEnemy(Enemy *enemy, float dt);

// 绘制敌人：source 为当前动画帧源矩形；按面朝方向水平翻转。
void DrawEnemy(Enemy *enemy, Rectangle source);

// 敌怪与玩家的碰撞：触碰置 isCountdown 进入 1s 定格窗口，满 1s 调用 onBattle；
// 玩家触碰敌怪不扣血。
void ePlayerCollision(Enemy *enemy, Player *player);

// 敌怪能够站在平台和矩形（地面）上
void eGroundCollision(Enemy *enemy);

// 敌怪与单向平台的碰撞：从上方落到平台顶面可站立（从下方/侧面穿过不响应）。
void ePlatformCollision(Enemy *enemy, Platform *platform);

#endif // ENEMY_H
