/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SCENE_SPELL_H
#define SCENE_SPELL_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"

/* 创建平台拼写关卡场景 (关卡类型 1 ,见docs/game_instructions.md 关卡设计)
 * 无须额外绘制矩形地面,用platform中的Large Type作为玩家的核心平台和落脚点.
 * 开放式空间,采用锁定镜头.游戏设计中的详情见(docs/game_instructions.md
 * 关卡设计) 参数:
 * app: 游戏应用 : 只读引用,不拥有
 * difficulty: 关卡难度
 * level: 关卡序号, 使用level_flow系统推进关卡
 */

GameScene *SpellSceneCreate(const GameApp *app, int difficulty, int level);

#endif // SCENE_SPELL_H