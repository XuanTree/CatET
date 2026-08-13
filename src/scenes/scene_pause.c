#include "scenes/scene_pause.h"
#include "tools/raygui.h"

typedef struct PauseData {
  const GameApp *app;
} PauseData;

static void PauseUpdate(GameScene *self, float dt) {
  // 暂停界面需要Update吗，应该是不需要吧，就这么放着吧
  return;
}

static void PauseDraw(GameApp *self) {
  DrawRectangle(self->logicWidth, self->logicHeight, GetScreenWidth(),
                GetScreenHeight(), Fade(LIGHTGRAY, 0.3f));
  DrawText("PAUSE", GetScreenWidth() / 2, GetScreenHeight(), 32, LIGHTGRAY);
  return;
}

GameScene *PauseSceneCreate(const GameApp *app) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  return scene;
}
