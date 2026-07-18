#include "game.h"
#include "raylib.h"
#include <sys/stat.h>

static const int LOGIC_WIDTH = 800;
static const int LOGIC_HEIGHT = 600;

void Run() {
  InitWindow(LOGIC_WIDTH, LOGIC_HEIGHT, "CatET");

  SetTargetFPS(60);
  InitAudioDevice();

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    EndDrawing();
  }

  CloseAudioDevice();
  CloseWindow();
}
