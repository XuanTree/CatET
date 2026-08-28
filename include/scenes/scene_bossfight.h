/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SCENE_BOSSFIGHT_H
#define SCENE_BOSSFIGHT_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"

/* 创建Boss战斗场景,(关卡类型 高难度关卡 见docs/game_instructions.md 关卡设计4)
 * 无须额外绘制矩形地面,用platform中的Large Type作为玩家落脚点和核心平台
 * 开放式空间,使用锁定镜头,boss在屏幕上方一定区域内来回飞行,周期性向玩家释放弹幕攻击
 * 玩家需要躲避弹幕攻击,同时完成单词拼写
 * 总共完成3次单词拼写后,判定为玩家胜利,删除boss,进入下一关(若为第100关,则判定玩家胜利,具体详细内容
 * 见docs/game_instructions.md 关卡设计4)
 * 参数:
 *  app: 游戏应用 : 框架只读引用,不拥有
 *  difficulty: 关卡难度 :难度0=简单,难度1=普通,难度2=困难
 *  level: 关卡序号 :当前关卡序号,通关后经level_flow系统推进
 */

GameScene *BossFightSceneCreate(const GameApp *app, int difficulty, int level);

#endif // SCENE_BOSSFIGHT_H