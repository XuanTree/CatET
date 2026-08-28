/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SCENE_MAZE_H
#define SCENE_MAZE_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"

// 创建迷宫解密场景（关卡类型 2，见 docs/game_instructions.md 关卡设计 2）。
// 侧视平台跳跃式封闭纵向迷宫以网格砌出四周
// 封闭的迷宫箱，内部为实心泥土挖出的蜿蜒隧道与竖井，玩家穿行其中寻找被
// 挖掉的正确字母并带回中央拼写平台拼写（拼写平台以外任意位置均可放下字母）。
// 拼写正确进入下一关；拼写错误扣血并重置字母；HP 归零判定失败。
// 参数：
//   - app        : 框架只读引用，不拥有
//   - difficulty : 难度 0=简单(CET4)、1=普通(CET4)、2=困难(CET6)。
//   - level      : 当前关卡编号（从 1 开始，通关后经 level_flow 推进）
GameScene *MazeSceneCreate(const GameApp *app, int difficulty, int level);

#endif // SCENE_MAZE_H
