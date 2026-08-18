#include "core/gameapp.h"

GameApp GameAppInit(const int logicWidth, const int logicHeight,
                    const char *title) {
  GameApp app = {0};
  app.logicWidth = logicWidth;
  app.logicHeight = logicHeight;

  // 允许窗口自由缩放
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(logicWidth, logicHeight, title);
  // 禁用 ESC 默认关闭窗口：ESC 交给主循环用于弹出暂停界面
  SetExitKey(0);
  // 游戏全局隐藏鼠标：开始/暂停/关卡界面均已支持纯键盘操作
  HideCursor();
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

  // UI 选中音效：开始/暂停菜单切换选中项时播放（由各场景 Update 触发）
  app.uiSound = LoadSound(
      TextFormat("%sassets/sounds/ui_sound.ogg", GetApplicationDirectory()));

  return app;
}

// 计算逻辑分辨率到窗口的等比缩放（含居中黑边）
// 逻辑分辨率：场景以固定逻辑分辨率绘制，raygui 的按钮矩形也位于该空间，
// 若直接用实际窗口像素坐标做命中检测，窗口放大后交互就会错位。
// raylib 的鼠标变换为 L = M*scale' + offset'，要得到 L = (M - 黑边)/scale，
// 但是其实我也不想让这游戏有什么鼠标操作。。。呃？
// 因此 scale' = 1/scale，offset' = -黑边/scale。
static float ApplyViewportScale(GameApp *app) {
  float screenW = (float)GetScreenWidth();
  float screenH = (float)GetScreenHeight();
  float scale = fminf(screenW / (float)app->logicWidth,
                      screenH / (float)app->logicHeight);
  float offsetX = (screenW - (float)app->logicWidth * scale) * 0.5f;
  float offsetY = (screenH - (float)app->logicHeight * scale) * 0.5f;

  SetMouseScale(1.0f / scale, 1.0f / scale);
  SetMouseOffset((int)roundf(-offsetX / scale), (int)roundf(-offsetY / scale));
  return scale;
}

void GameAppBegin(GameApp *app) {
  // 先同步鼠标到逻辑坐标，保证本帧 GUI（raygui）命中检测准确
  ApplyViewportScale(app);
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
  float scale = ApplyViewportScale(app);
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
  UnloadSound(app->uiSound);
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