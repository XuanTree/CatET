/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SCENE_START_H
#define SCENE_START_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"
#include "extern/raygui.h"
#include <raylib.h>

// 创建开始菜单场景：标题 + 开始/设置/退出按钮。
// 点击 Play 切换至测试关卡场景，点击 Quit 退出游戏。
GameScene *StartSceneCreate(GameApp *app);

#endif // !SCENE_START_H
