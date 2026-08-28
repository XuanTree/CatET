/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef GAMESTACK_H
#define GAMESTACK_H

#pragma once
#include <stdbool.h>
#include <stdlib.h>

// ─────────────────────────────────────────────────────────────────────────────
// 游戏栈（Game Stack）核心：
//   以「场景（Scene）+ 栈（Stack）」管理游戏状态，栈顶场景为唯一活跃场景，
//   被压入的新场景可覆盖在旧场景之上（如暂停层盖在关卡之上），
//   弹出后自动恢复下层场景。为未来的开始界面、多关卡、暂停、结算等场景做准备。
// ─────────────────────────────────────────────────────────────────────────────

typedef struct GameScene GameScene;

typedef void (*SceneUpdateFn)(GameScene *scene, float dt);
typedef void (*SceneDrawFn)(GameScene *scene);
typedef void (*SceneEventFn)(GameScene *scene);

typedef enum GameSceneFlags {
  GAME_SCENE_NONE = 0,
  GAME_SCENE_UPDATE_WHEN_HIDDEN = 1 << 0, // 被覆盖时仍更新（背景、粒子）
  GAME_SCENE_DRAW_WHEN_HIDDEN = 1 << 1,   // 被覆盖时仍绘制（暂停的半透明底层）
} GameSceneFlags;

// 栈核心采用不透明结构，外部只通过 API 操作（前向声明，供 GameScene.owner
// 使用）
typedef struct GameStack GameStack;

// 场景：一个可独立运行的最小游戏单元（菜单、关卡、暂停、结算都算场景）。
// 通过工厂函数（如 TestSceneCreate）创建，栈内 malloc；
// 销毁由 GameStack 统一负责（先 onExit 再 free，data 一并释放）。
struct GameScene {
  const char *name;       // 场景名（调试用）
  SceneEventFn onEnter;   // 进入：加载本场景资源
  SceneEventFn onExit;    // 永久离开：卸载本场景资源
  SceneUpdateFn onUpdate; // 每帧更新（仅当活跃或被允许隐藏更新）
  SceneDrawFn onDraw;     // 每帧绘制（仅当活跃或被允许隐藏绘制）
  SceneEventFn onPause;   // 被新场景覆盖时触发（可为 NULL）
  SceneEventFn onResume;  // 重新回到栈顶时触发（可为 NULL）
  void *data;             // 场景私有数据（栈负责释放）
  GameSceneFlags flags;
  GameStack *owner; // 所属栈：压入时由 GameStack 注入，场景回调可据此切换
  bool pauseable;   // 是否允许按 ESC 调出暂停界面（开始界面等菜单设为 false）
};
// 生命周期
GameStack *GameStackCreate(void);
void GameStackDestroy(GameStack *stack);

// 场景切换请求：这些 API 不会立即修改栈，而是写入延迟请求队列，
// 由 GameStackUpdate 在下一帧帧首统一应用，从而保证场景回调中任意
// 时机调用切换都是安全的（不会在迭代中被释放）。
void GameStackPush(GameStack *stack, GameScene *scene); // 压入（覆盖当前栈顶）
void GameStackPop(GameStack *stack);                    // 弹出（恢复下层）
void GameStackReplace(GameStack *stack,
                      GameScene *scene); // 替换栈顶（关卡跳转）
void GameStackClearTo(GameStack *stack, GameScene *scene); // 清空到仅剩该场景

// 查询
GameScene *GameStackTop(GameStack *stack);
int GameStackSize(GameStack *stack);
bool GameStackEmpty(GameStack *stack);

// 每帧驱动：帧首 flush 延迟请求，再按“栈顶 → 底层”更新场景
void GameStackUpdate(GameStack *stack, float dt);
// 每帧绘制：按“底层 → 栈顶”顺序绘制（由调用方包在 GameAppBegin/End 之间）
void GameStackDraw(GameStack *stack);

// 退出请求：主循环据此结束
void GameStackRequestQuit(GameStack *stack);
bool GameStackWantsQuit(GameStack *stack);

#endif // GAMESTACK_H
