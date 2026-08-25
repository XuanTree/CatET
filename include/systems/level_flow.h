#ifndef LEVEL_FLOW_H
#define LEVEL_FLOW_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"

// ─────────────────────────────────────────────────────────────────────────────
// 关卡流程（系统层）：负责跨关卡推进与关卡类型权重刷新。
// 设计依据（docs/game_instructions.md 关卡刷新权重）：平台跳跃为主、极速拼写
// 次之、迷宫解密最少（权重比 5:3:2）。当前「极速拼写」关卡尚未实现，其权重
// 并入平台跳跃侧，保证「平台跳跃为核心玩法」优先落地：
//   平台跳跃 ~70%，迷宫解密 ~30%。
// ─────────────────────────────────────────────────────────────────────────────

typedef enum LevelType {
  LEVEL_TYPE_PLATFORM = 0, // 平台跳跃（核心玩法）
  LEVEL_TYPE_MAZE,         // 迷宫解密
  LEVEL_TYPE_COUNT
} LevelType;

// 依据权重随机返回下一关类型（平台跳跃为主）。
LevelType LevelFlowRollType();

// 生成指定关卡（level/difficulty）与类型的场景；type 非法时回退平台跳跃。
GameScene *LevelFlowCreateScene(const GameApp *app, LevelType type, int level,
                                int difficulty);

// 便捷：生成「下一关」场景（currentLevel 基础上 +1，类型按权重刷新）。
GameScene *LevelFlowCreateNextScene(const GameApp *app, int currentLevel,
                                    int difficulty);

#endif // LEVEL_FLOW_H
