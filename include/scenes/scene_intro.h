/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SCENE_INTRO_H
#define SCENE_INTRO_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"

// 创建「启动名言」转场场景：黑底白字显示一句随机名言（取 systems/dialogue 的
// getStartText()），3 秒后自动（或按 X / Z / Enter / Space 跳过）经通用过渡
// 场景进入主菜单（scene_start）。作为游戏启动后的首个场景，避免玩家一启动就
// 直接进入菜单并播放音乐的突兀感；本场景自身全程静音（不播放 BGM）。
//
// 参数：
//   app —— 框架引用（只读引用，不拥有）
GameScene *IntroSceneCreate(GameApp *app);

#endif // SCENE_INTRO_H
