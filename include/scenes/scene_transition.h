/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SCENE_TRANSITION_H
#define SCENE_TRANSITION_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"

// 创建通用过渡场景：显示「淡入-保持-淡出」黑色遮罩动画后，自动 Replace 到
// next 目标场景。作为菜单→关卡、关卡→关卡、关卡→菜单三种转场的通用组件
// 复用（采用 Replace 模式，栈内不残留源场景）。
// 参数：
//   - app : 框架只读引用，不拥有
//   - next: 过渡结束后要切换到的目标场景（所有权转移给本场景，由超时
//           Replace 消费；若本场景被异常 Pop，onExit 负责释放防泄漏）
// 用法示例（均需先 include 对应目标场景头文件）：
//   菜单→关卡：GameStackReplace(owner,
//                TransitionSceneCreate(app, LevelSceneCreate(app, 1)));
//   关卡→关卡：GameStackReplace(owner,
//                TransitionSceneCreate(app, LevelSceneCreate(app, 2)));
//   关卡→菜单：GameStackReplace(owner,
//                TransitionSceneCreate(app, StartSceneCreate(app)));
GameScene *TransitionSceneCreate(const GameApp *app, GameScene *next);

// 创建「淡出后 Pop」过渡场景：显示「淡入-保持-淡出」黑色遮罩动画后，Pop
// 自身并露出下层场景。用于覆盖层场景（如战斗场景）退出时的转场：
//   战斗胜利 → GameStackReplace(owner, TransitionSceneCreatePop(app))
//   先以转场覆盖层替换战斗场景，淡出结束后 Pop 露出下层平台关卡。
GameScene *TransitionSceneCreatePop(const GameApp *app);

#endif // SCENE_TRANSITION_H
