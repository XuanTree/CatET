#include "game.h"
#include <raylib.h>
#include <stdlib.h>

// 创建战斗场景私有化数据
typedef struct BattleSceneData {
  const GameApp *app;
  Player cat;         // 玩家
  SceneCamera camera; // 锁定镜头
  Enemy enemy;

  int difficulty;
  Rectangle source;    // 动画绘制源矩形
  Character character; // 单词挑选玩法
} BattleSceneData;

static void BattleSceneEnter(GameScene *self) {}

static void BattleSceneDraw(GameScene *self) {}

static void BattleSceneUpdate(GameScene *self, float dt) {}

static void BattleSceneExit(GameScene *self) {}

GameScene *BattleSceneCreate(const GameApp *app, int difficulty) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  if (scene == NULL) {
    return NULL;
  }
  BattleSceneData *data = (BattleSceneData *)calloc(1, sizeof(BattleSceneData));
  if (data == NULL) {
    free(scene);
    return NULL;
  }
  // 战斗场景基本信息
  data->app = app;
  data->difficulty = difficulty;

  scene->name = "BattleScene";
  scene->data = data;
  scene->pauseable = false; // 战斗场景不允许暂停

  scene->onEnter = BattleSceneEnter;
  scene->onDraw = BattleSceneDraw;
  scene->onUpdate = BattleSceneUpdate;
  scene->onExit = BattleSceneExit;

  return scene;
}