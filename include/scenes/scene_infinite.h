/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SCENE_INFINITE_H
#define SCENE_INFINITE_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"
#include <raylib.h>

// 创建无尽单词拼写场景（主菜单 Play 下的 Infinite 入口，难度选择后进入）：
//   面向「只想刷单词、不想打关卡」的玩家 —— 移除敌怪/弹幕的战斗环节，
//   是「没有敌人的战斗场景」：黑底网格舞台 + 单词选择玩法。每轮三选一
//   （给词性 + 中文释义，从三个英文单词中选出匹配项；干扰词与答案词性
//   相同、长度相近），答对计 1 分并出下一题（不回血），答错按难度扣血；
//   玩家 HP 归零本局结束，进入本场景自带的结算界面（重开/回菜单/退出）。
//   学习机制：全程接入全局错词本/间隔重复抽词（systems/study_tracker），
//   答对/答错自动标记，拼错的词按 STUDY_REVISIT_INTERVAL 题间隔后复现；
//   答错后停留展示正确答案与释义（复习横幅），玩家按 Z 继续下一题。
//   最佳成绩（单局最高答对数）独立持久化（systems/save_data 的
//   infiniteBest 字段），与主线速通纪录互不干扰。
// 参数：
//   app        —— 框架引用（不拥有）
//   difficulty —— 难度 0/1/2（词库与惩罚分级，同其它模式：0/1=CET4、2=CET6）
// 说明：玩家精灵（Player）由本场景自己创建并持有（自成一个游玩单元），
// 不沿用 BattleScene 的“外部传入玩家指针”模式 —— 无尽模式从主菜单进入，
// 没有上层关卡场景。
GameScene *InfiniteSceneCreate(const GameApp *app, int difficulty);

#endif // SCENE_INFINITE_H
