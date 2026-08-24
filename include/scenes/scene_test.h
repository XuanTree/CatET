#ifndef SCENE_TEST_H
#define SCENE_TEST_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"

// 创建「平台跳跃」关卡场景（docs/game_instructions.md 关卡设计 3，核心玩法）：
// 玩家在平台与地面上跳跃前进，触碰终点小红旗即通关，进入下一关。
// 参数：
//   - app        : 框架只读引用，不拥有
//   - level      : 当前关卡编号（从 1 开始，通关后经 level_flow 推进）
//   - difficulty : 难度 0=简单(CET4) 1=普通(CET4) 2=困难(CET6)
GameScene *TestSceneCreate(const GameApp *app, int level, int difficulty);

#endif // SCENE_TEST_H
