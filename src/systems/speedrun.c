#include "game.h"

void SpeedrunInit(GameApp *app) {
  if (app == NULL)
    return;
  app->bestTime = SaveDataLoadBestTime();
  app->speedrunElapsed = 0.0f;
  app->speedrunActive = false;
}

void SpeedrunStart(GameApp *app) {
  if (app == NULL)
    return;
  app->speedrunElapsed = 0.0f;
  app->speedrunActive = true;
}

void SpeedrunStop(GameApp *app) {
  if (app == NULL)
    return;
  app->speedrunActive = false;
}

void SpeedrunTick(GameApp *app, float dt) {
  if (app == NULL || !app->speedrunActive)
    return;
  app->speedrunElapsed += dt;
}

bool SpeedrunFinish(GameApp *app) {
  if (app == NULL)
    return false;
  SpeedrunStop(app);
  const bool isRecord =
      (app->bestTime < 0.0f || app->speedrunElapsed < app->bestTime);
  if (isRecord) {
    app->bestTime = app->speedrunElapsed;
    SaveDataSaveBestTime(app->bestTime);
  }
  return isRecord;
}
