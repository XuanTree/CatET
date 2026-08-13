#include "core/gameapp.h"

GameApp GameAppInit(const int logicWidth, const int logicHeight,
                    const char *title) {
  GameApp app = {0};
  app.logicWidth = logicWidth;
  app.logicHeight = logicHeight;

  // 允许窗口自由缩放
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(logicWidth, logicHeight, title);
  // 限制最小窗口尺寸，避免被压缩得过小
  SetWindowMinSize(logicWidth, logicHeight);

  // 窗口图标
  Image icon = LoadImage(
      TextFormat("%sassets/sprites/icon.png", GetApplicationDirectory()));
  SetWindowIcon(icon);
  app.icon = icon; // 保留以便最后卸载

  // 固定分辨率渲染目标：内部始终按逻辑分辨率渲染，防止放大后画面模糊
  app.target = LoadRenderTexture(logicWidth, logicHeight);
  // 双线性过滤，让缩放后的画面保持平滑清晰
  SetTextureFilter(app.target.texture, TEXTURE_FILTER_BILINEAR);

  app.isPaused = false;
  SetTargetFPS(60);
  InitAudioDevice();

  return app;
}

void GameAppBegin(GameApp *app) {
  BeginTextureMode(app->target);
  ClearBackground(RAYWHITE);
}

void GameAppEnd(GameApp *app) {
  (void)app;
  EndTextureMode();
}

void GameAppPresent(GameApp *app) {
  BeginDrawing();
  ClearBackground(BLACK);

  float screenW = (float)GetScreenWidth();
  float screenH = (float)GetScreenHeight();
  float scale = fminf(screenW / (float)app->logicWidth,
                      screenH / (float)app->logicHeight);
  float destW = (float)app->logicWidth * scale;
  float destH = (float)app->logicHeight * scale;
  Rectangle dest = {
      .x = (screenW - destW) * 0.5f,
      .y = (screenH - destH) * 0.5f,
      .width = destW,
      .height = destH,
  };
  // RenderTexture 的纹理在 OpenGL 中上下颠倒，source 高度取负
  Rectangle sourceTex = {0, 0, (float)app->logicWidth,
                         -(float)app->logicHeight};
  DrawTexturePro(app->target.texture, sourceTex, dest, (Vector2){0, 0}, 0.0f,
                 WHITE);

  EndDrawing();
}

void GameAppPollGlobalInput(void) {
  // F11 或 Alt+Enter 切换全屏
  if (IsKeyPressed(KEY_F11) ||
      (IsKeyDown(KEY_LEFT_ALT) && IsKeyPressed(KEY_ENTER))) {
    ToggleFullscreen();
  }
}

void GameAppClose(GameApp *app) {
  UnloadRenderTexture(app->target);
  UnloadImage(app->icon);
  CloseAudioDevice();
  CloseWindow();
}

void GameAppPaused(GameApp *app) {
  if (IsKeyDown(KEY_ESCAPE)) {
    app->isPaused = true;
  }
}

void GameAppResume(GameApp *app) {
  if (IsKeyDown(KEY_ESCAPE)) {
    app->isPaused = false;
  }
}