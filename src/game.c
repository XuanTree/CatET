#include "game.h"
#include "player.h"
#include "raylib.h"
#include <math.h>

// 逻辑分辨率固定不变，窗口放大时通过 RenderTexture 等比缩放，画面不变糊
#define LOGIC_WIDTH 640
#define LOGIC_HEIGHT 480

void Run() {
  // 允许窗口自由缩放
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(LOGIC_WIDTH, LOGIC_HEIGHT, "CatET");
  // 限制最小窗口尺寸，避免被压缩得过小
  SetWindowMinSize(LOGIC_WIDTH, LOGIC_HEIGHT);

  Image iconImage = LoadImage(
      TextFormat("%sassets/sprites/icon.png", GetApplicationDirectory()));
  SetWindowIcon(iconImage);

  // 固定分辨率渲染目标：内部始终按 800x600 渲染，防止放大后画面模糊
  RenderTexture target = LoadRenderTexture(LOGIC_WIDTH, LOGIC_HEIGHT);
  // 双线性过滤，让缩放后的画面保持平滑清晰
  SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

  Player cat;
  InitPlayer(&cat);

  SetTargetFPS(60);
  InitAudioDevice();

  while (!WindowShouldClose()) {
    // F11 或 Alt+Enter 切换全屏
    if (IsKeyPressed(KEY_F11) ||
        (IsKeyDown(KEY_LEFT_ALT) && IsKeyPressed(KEY_ENTER))) {
      ToggleFullscreen();
    }

    // 更新逻辑
    float dt = GetFrameTime();
    UpdatePlayer(&cat, dt);
    GroundCollision(&cat);

    // 更新动画
    Rectangle source =
        AnimationUpdate(&cat.animations[cat.playerAnimationState], dt);

    // 1. 先绘制到固定分辨率渲染目标
    BeginTextureMode(target);
    ClearBackground(RAYWHITE);

    // 绘制玩家
    DrawPlayer(&cat, source);

    // 绘制地面
    DrawRectangle(0, LOGIC_HEIGHT - 50, LOGIC_WIDTH, 50, LIGHTGRAY);

    EndTextureMode();

    // 2. 将渲染结果等比缩放到整个窗口（保持宽高比并居中，多余区域用黑边）
    BeginDrawing();
    ClearBackground(BLACK);

    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float scale = fminf(screenW / LOGIC_WIDTH, screenH / LOGIC_HEIGHT);
    float destW = LOGIC_WIDTH * scale;
    float destH = LOGIC_HEIGHT * scale;
    Rectangle dest = {
        .x = (screenW - destW) * 0.5f,
        .y = (screenH - destH) * 0.5f,
        .width = destW,
        .height = destH,
    };
    // 注意：RenderTexture 的纹理在 OpenGL 中是上下颠倒的，source 高度取负
    Rectangle sourceTex = {0, 0, LOGIC_WIDTH, -LOGIC_HEIGHT};
    DrawTexturePro(target.texture, sourceTex, dest, (Vector2){0, 0}, 0.0f,
                   WHITE);

    EndDrawing();
  }

  UnloadRenderTexture(target);
  UnloadImage(iconImage);
  CloseAudioDevice();
  CloseWindow();
}
