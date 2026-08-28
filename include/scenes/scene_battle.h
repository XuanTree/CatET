/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SCENE_BATTLE_H
#define SCENE_BATTLE_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"
#include "entities/enemy.h"
#include "entities/player.h"
#include <raylib.h>

// 创建战斗场景（回合制，docs/game_instructions.md §战斗场景）：
//   共 3 个回合（不论选对次数）：每回合 = 玩家回合（3 选 1 选词，选错扣血）
//   + 敌怪回合（玩家在平台上移动躲避弹幕，所有弹幕消失后本回合结束）。
//   满 3 回合即胜利：画面停顿 0.8s（预留胜利音效播放窗口）后
//   enemy->isAlive=false（“删除该敌怪”），再经淡出转场 Pop 回 returnScene；
//   玩家 HP 归零：失败，Replace 到 FailScene。
// 经转场 GameStackPush
// 压入平台跳跃关卡之上（进入/退出均有转场）；pauseable=false。 参数：
//   player      —— 平台关卡的真实玩家指针（战斗直接改其 HP/位置/动画）
//   enemy       —— 平台关卡的真实敌怪指针（冻结 isMovable、显示、胜利后删除）
//   returnScene —— 战斗结束返回的关卡场景（胜利时 Pop 回到它）
//   difficulty  —— 难度 0/1/2：影响拼写错误惩罚（×1.5^d）与弹幕伤害（×2^d）
GameScene *BattleSceneCreate(const GameApp *app, Player *player, Enemy *enemy,
                             GameScene *returnScene, int difficulty);

#endif // SCENE_BATTLE_H
