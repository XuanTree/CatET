/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SCENE_SETTINGS_H
#define SCENE_SETTINGS_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"

// 创建设置场景（覆盖层）：音效/音乐总开关。
// 作为覆盖层 Push 到开始菜单之上（下层 StartScene 标记 DRAW_WHEN_HIDDEN，
// 半透明遮罩下透出主菜单底）；X / ESC 键 Pop 返回。
// 暂停界面暂不提供设置入口（主循环 ESC 状态机只识别 PauseScene 覆盖层，
// 多层覆盖会破坏 isPaused 状态，见 src/game.c 的 Run 注释）。
GameScene *SettingsSceneCreate(const GameApp *app);

#endif // SCENE_SETTINGS_H
