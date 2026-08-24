#include "systems/level_flow.h"
#include "scenes/scene_maze.h"
#include "scenes/scene_test.h"
#include "tools/genrandom.h"

// 迷宫权重边界：随机数 [0,100) 中小于 30 时为迷宫（~30%），
// 其余为平台跳跃（~70%），保证平台跳跃为核心玩法。
#define MAZE_ROLL_BOUND 30

LevelType LevelFlowRollType(void) {
  return (genRandomNum(100) < MAZE_ROLL_BOUND) ? LEVEL_TYPE_MAZE
                                               : LEVEL_TYPE_PLATFORM;
}

GameScene *LevelFlowCreateScene(const GameApp *app, LevelType type, int level,
                                int difficulty) {
  if (type == LEVEL_TYPE_MAZE) {
    return MazeSceneCreate(app, difficulty, level);
  }
  // 默认（含非法类型回退）：生成平台跳跃关卡
  return TestSceneCreate(app, level, difficulty);
}

GameScene *LevelFlowCreateNextScene(const GameApp *app, int currentLevel,
                                    int difficulty) {
  return LevelFlowCreateScene(app, LevelFlowRollType(), currentLevel + 1,
                              difficulty);
}
