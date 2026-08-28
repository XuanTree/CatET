#include "game.h"

// 关卡类型权重边界（docs 关卡刷新权重 5:3:2 = 平台:拼写:迷宫）：
//   [0, MAZE)      → 迷宫解密（~20%）
//   [MAZE, SPELL)  → 极速拼写（~30%）
//   [SPELL, TOWER) → 平台爬塔（~15%，平台子类）
//   [TOWER, 100)   → 经典平台（~35%）；平台合计 ~50% 为主玩法
#define MAZE_ROLL_BOUND 20
#define SPELL_ROLL_BOUND 50
#define TOWER_ROLL_BOUND 65

LevelType LevelFlowRollType(void) {
  int roll = genRandomNum(100);
  if (roll < MAZE_ROLL_BOUND)
    return LEVEL_TYPE_MAZE; // ~20% 迷宫解密
  if (roll < SPELL_ROLL_BOUND)
    return LEVEL_TYPE_SPELL; // ~30% 极速拼写
  if (roll < TOWER_ROLL_BOUND)
    return LEVEL_TYPE_PLATFORM_TOWER; // ~15% 平台爬塔
  return LEVEL_TYPE_PLATFORM;         // ~35% 经典平台
}

GameScene *LevelFlowCreateScene(const GameApp *app, LevelType type, int level,
                                int difficulty) {
  // 高难度关卡：固定每 20 关刷新一次（第 20/40/60/80/100 关），
  // 覆盖类型权重刷新（docs 关卡设计 4）
  if (level > 0 && level % 20 == 0)
    return BossFightSceneCreate(app, difficulty, level);
  if (type == LEVEL_TYPE_MAZE) {
    return MazeSceneCreate(app, difficulty, level);
  }
  if (type == LEVEL_TYPE_SPELL) {
    return SpellSceneCreate(app, difficulty, level);
  }
  // 平台跳跃（经典平台 LEVEL_TYPE_PLATFORM / 爬塔 LEVEL_TYPE_PLATFORM_TOWER）
  // 统一使用正式平台场景 PlatformSceneCreate；scene_test 为测试关卡，
  // 不参与正式游玩流程。默认（含非法类型）同样回退到正式平台场景。
  return PlatformSceneCreate(app, difficulty, level);
}

GameScene *LevelFlowCreateNextScene(const GameApp *app, int currentLevel,
                                    int difficulty) {
  return LevelFlowCreateScene(app, LevelFlowRollType(), currentLevel + 1,
                              difficulty);
}
