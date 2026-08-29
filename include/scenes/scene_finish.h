/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SCENE_FINISH_H
#define SCENE_FINISH_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"

// 创建通关结算场景：当玩家成功通关游戏（第 MAX_LEVELS 关）时由关卡场景
// 经过渡场景（TransitionScene）进入，显示「Congratulations!」标题与本轮
// 通关所花费的时间（速通计时，见 systems/speedrun）。
// 玩家只能选择「回到菜单」（清空场景栈返回开始界面）一项。
GameScene *FinishSceneCreate(const GameApp *app);

#endif // !SCENE_FINISH_H
