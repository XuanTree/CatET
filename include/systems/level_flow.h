/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef LEVEL_FLOW_H
#define LEVEL_FLOW_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"

// ─────────────────────────────────────────────────────────────────────────────
// 关卡流程（系统层）：负责跨关卡推进与关卡类型权重刷新。
// 设计依据（docs/game_instructions.md 关卡刷新权重）：平台跳跃为主、极速拼写
// 次之、迷宫解密最少（权重比 5:3:2 = 平台:拼写:迷宫）：
//   经典平台 ~35%，爬塔 ~15%（平台合计 ~50%），极速拼写 ~30%，迷宫 ~20%。
// 高难度关卡（Boss 战）固定每 20 关刷新（第 20/40/60/80/100 关），
// 覆盖类型权重刷新（见 LevelFlowCreateScene）。
// ─────────────────────────────────────────────────────────────────────────────

typedef enum LevelType {
  LEVEL_TYPE_PLATFORM = 0,   // 平台跳跃（核心玩法）
  LEVEL_TYPE_PLATFORM_TOWER, // 平台跳跃·爬塔（随机平台+顶部红旗）
  LEVEL_TYPE_MAZE,           // 迷宫解密
  LEVEL_TYPE_SPELL,          // 极速拼写
  LEVEL_TYPE_COUNT
} LevelType;

// 依据权重随机返回下一关类型（平台跳跃为主）。
LevelType LevelFlowRollType();

// 生成指定关卡（level/difficulty）与类型的场景；type 非法时回退平台跳跃。
GameScene *LevelFlowCreateScene(const GameApp *app, LevelType type, int level,
                                int difficulty);

// 便捷：生成「下一关」场景（currentLevel 基础上 +1，类型按权重刷新）。
GameScene *LevelFlowCreateNextScene(const GameApp *app, int currentLevel,
                                    int difficulty);

#endif // LEVEL_FLOW_H
