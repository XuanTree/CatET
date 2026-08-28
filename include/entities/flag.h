/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef FLAG_H
#define FLAG_H

#pragma once
#include <raylib.h>

// ─────────────────────────────────────────────────────────────────────────────
// 平台跳跃关卡终点小红旗：关卡通关标志（docs/game_instructions.md 关卡设计
// 3）。 程序化绘制（旗杆 + 红旗三角 + 底座 + 顶部圆点），无需额外贴图资源；
// 玩家触碰红旗即判定该关胜利，进入下一关。
// ─────────────────────────────────────────────────────────────────────────────

typedef struct Flag {
  Vector2 base;     // 旗杆底部中心（世界坐标，通常立于地面顶面）
  Rectangle hitbox; // 碰撞盒：覆盖旗杆主体，玩家矩形与之相交即通关
} Flag;

// 初始化红旗：将旗杆底部中心置于 base。
void InitFlag(Flag *flag, Vector2 base);

// 绘制红旗（世界坐标，需在场景相机内调用）。
void DrawFlag(const Flag *flag);

// 玩家矩形与红旗碰撞盒是否相交（触发通关判定）。
bool FlagCheckCollision(const Flag *flag, Rectangle playerRect);

#endif // FLAG_H
