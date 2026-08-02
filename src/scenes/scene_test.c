#include "scenes/scene_test.h"
#include "platform.h"
#include "player.h"
#include "raylib.h"
#include <stdlib.h>

// 场景私有数据：栈持有并负责释放
typedef struct TestData {
  const GameApp *app; // 只读引用，不拥有
  Player cat;
  Platform platform;
  Platform platform_m;
  Rectangle source; // 当前动画帧源矩形
} TestData;

static void TestEnter(GameScene *self) {
  TestData *d = (TestData *)self->data;
  // 零初始化：杜绝未初始化内存导致的未定义行为（UB）。
  // 否则 platformTexture.id 等字段可能残留垃圾值，Debug/Release 表现不一致
  d->cat = (Player){0};
  InitPlayer(&d->cat);
  d->platform = (Platform){0};
  InitJumpPlatforms(&d->platform, (Vector2){100, 350}, SMALL);
  d->platform_m = (Platform){0};
  InitJumpPlatforms(&d->platform_m, (Vector2){300, 200}, MEDIUM);
}

static void TestUpdate(GameScene *self, float dt) {
  TestData *d = (TestData *)self->data;

  UpdatePlayer(&d->cat, dt);

  // 每帧先重置着地标记，再检测平台/地面碰撞。
  // 若不重置，离开平台边缘后 isOnTheGround 仍为 true，玩家会悬空。
  d->cat.isOnTheGround = false;
  PlayerCollision(&d->cat, &d->platform, dt);
  PlayerCollision(&d->cat, &d->platform_m, dt);
  GroundCollision(&d->cat);

  // 更新动画（记录当前帧源矩形供绘制使用）
  d->source =
      AnimationUpdate(&d->cat.animations[d->cat.playerAnimationState], dt);
}

static void TestDraw(GameScene *self) {
  TestData *d = (TestData *)self->data;

  // 绘制平台
  DrawPlatform(&d->platform);
  DrawPlatform(&d->platform_m);

  // 绘制玩家
  DrawPlayer(&d->cat, d->source);

  // 绘制地面（高度与逻辑分辨率一致）
  DrawRectangle(0, d->app->logicHeight - 50, d->app->logicWidth, 50, LIGHTGRAY);
}

static void TestExit(GameScene *self) {
  TestData *d = (TestData *)self->data;
  // 卸载本场景加载的资源（与场景生命周期绑定）
  UnloadTexture(d->cat.idleTexture);
  UnloadTexture(d->cat.runTexture);
  UnloadTexture(d->cat.jumpTexture);
  UnloadTexture(d->cat.sleepTexture);
  if (d->platform.platformTexture.id != 0) {
    UnloadTexture(d->platform.platformTexture);
  }
}

GameScene *TestSceneCreate(const GameApp *app) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  TestData *data = (TestData *)calloc(1, sizeof(TestData));
  data->app = app;

  scene->name = "TestScene";
  scene->data = data;
  scene->flags = GAME_SCENE_NONE;
  scene->onEnter = TestEnter;
  scene->onUpdate = TestUpdate;
  scene->onDraw = TestDraw;
  scene->onExit = TestExit;
  // onPause / onResume 暂不需要，保持 NULL
  return scene;
}
