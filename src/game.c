#include "game.h"
#include "player.h"
#include "raylib.h"

static int LOGIC_WIDTH = 800;
static int LOGIC_HEIGHT = 600;

void Run() {
  InitWindow(LOGIC_WIDTH, LOGIC_HEIGHT, "CatET");
  Player cat;
  InitPlayer(&cat);

  SetTargetFPS(60);
  InitAudioDevice();

  while (!WindowShouldClose()) {
    // 更新逻辑
    float dt = GetFrameTime();
    UpdatePlayer(&cat, dt);
    GroundCollision(&cat);

    // 更新动画
    Rectangle source =
        AnimationUpdate(&cat.animations[cat.playerAnimationState], dt);

    // 开始绘制
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // 绘制玩家
    DrawPlayer(&cat, source);

    // 绘制地面
    DrawRectangle(0, LOGIC_HEIGHT - 50, LOGIC_WIDTH, 50, LIGHTGRAY);

    // 绘制玩家（根据朝向翻转）

    EndDrawing();
  }

  CloseAudioDevice();
  CloseWindow();
}
