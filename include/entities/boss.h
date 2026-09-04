/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef BOSS_H
#define BOSS_H

#pragma once
#include "tools/animation.h"
#include "tools/timer.h"
#include <raylib.h>
#include <stdbool.h>

// 前置声明 Bullet：ShootBullet 仅以指针形式使用 Bullet，只需不完整类型。
// 不再 include "entities/bullet.h"，以打破 boss.h <-> bullet.h 的循环包含
// （这正是 "unknown type name 'Bullet'" 编译错误的根因）。
typedef struct Bullet Bullet;

typedef enum BossAnimation { BOSS_MOVE, BOSS_COUNT } BossAnimation;

// boss 不受重力控制->可飞行, boss 本身与玩家碰撞也不会造成伤害
// boss 全程会发射弹幕(bullet),bullet会对玩家造成碰撞伤害,碰撞造成伤害后
// 删除bullet
//! 注意绘制时优先绘制boss再绘制玩家,确保玩家图层在boss上方
typedef struct Boss {
  int hp;

  Texture2D bossTexture; // assests/sprites/boss.png , 就这一张(8帧数)
  Animation animations[BOSS_COUNT];

  // 移动/物理
  Vector2 position; // 世界坐标左上角
  Vector2 velocity; // 世界坐标速度
  Vector2 size;     // 世界坐标尺寸, boss尺寸可以...适当的放大一点

  Timer cooldownTimer; // boss的发射弹幕间隔计时器

  // 状态标记
  bool isCoolDown;
  bool isMovable;
  bool isAlive;
  bool isFacingRight;
} Boss;

void InitBoss(Boss *boss, Vector2 spawn_pos);

void UpdateBoss(Boss *boss, float dt);

void DrawBoss(Boss *boss, Rectangle source);

void ShootBullet(Boss *boss, Bullet *bullet);

#endif //! BOSS_H