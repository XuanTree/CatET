#include "game.h"

// 迷宫权重边界：随机数 [0,100) 中小于 30 时为迷宫（~30%），
// 其余为平台跳跃（~70%），保证平台跳跃为核心玩法。
#define MAZE_ROLL_BOUND 30
#define TOWER_ROLL_BOUND 65 // 迷宫 30 + 爬塔 35

LevelType LevelFlowRollType(void) {
  int roll = genRandomNum(100);
  if (roll < MAZE_ROLL_BOUND)
    return LEVEL_TYPE_MAZE; // ~30% 迷宫解密
  if (roll < TOWER_ROLL_BOUND)
    return LEVEL_TYPE_PLATFORM_TOWER; // ~35% 平台爬塔
  return LEVEL_TYPE_PLATFORM;         // ~35% 经典平台
}

GameScene *LevelFlowCreateScene(const GameApp *app, LevelType type, int level,
                                int difficulty) {
  if (type == LEVEL_TYPE_MAZE) {
    return MazeSceneCreate(app, difficulty, level);
  }
  if (type == LEVEL_TYPE_PLATFORM_TOWER) {
    return PlatformSceneCreate(app, difficulty, level);
  }
  // 默认（含非法类型回退）：生成经典平台跳跃关卡
  return TestSceneCreate(app, level, difficulty);
}

GameScene *LevelFlowCreateNextScene(const GameApp *app, int currentLevel,
                                    int difficulty) {
  return LevelFlowCreateScene(app, LevelFlowRollType(), currentLevel + 1,
                              difficulty);
}
