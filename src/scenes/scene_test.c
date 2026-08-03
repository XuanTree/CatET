#include "scenes/scene_test.h"
#include "platform.h"
#include "player.h"
#include "raylib.h"
#include "tools/camera.h"
#include <stdbool.h>
#include <stdlib.h>

// 场景私有数据：栈持有并负责释放
typedef struct TestData {
  const GameApp *app; // 只读引用，不拥有
  Player cat;
  SceneCamera sceneCamera; // 场景相机：由本场景持有并决定启用/禁用
  Platform platform;
  Platform platform_m;
  Rectangle source; // 当前动画帧源矩形
} TestData;

static void TestEnter(GameScene *self) {
  TestData *d = (TestData *)self->data;
  // 零初始化：杜绝未初始化内存导致的未定义行为。
  // 否则 platformTexture.id 等字段可能残留垃圾值
  d->cat = (Player){0};
  InitPlayer(&d->cat);
  d->platform = (Platform){0};
  InitJumpPlatforms(&d->platform, (Vector2){100, 350}, SMALL);
  d->platform_m = (Platform){0};
  InitJumpPlatforms(&d->platform_m, (Vector2){300, 200}, MEDIUM);

  // 初始化场景相机
  InitSceneCamera(&d->sceneCamera, d->app->logicWidth, d->app->logicHeight,
                  true, CAMERA_FOLLOW_CENTER);
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

  // 每帧把玩家位置设为相机跟随目标，再按场景配置的模式更新相机。
  // 若场景禁用相机（DisableSceneCamera），此处不会更新，保持固定视野。
  SetCameraTarget(&d->sceneCamera, d->cat.position);
  UpdateSceneCamera(&d->sceneCamera, dt);

  // 更新动画（记录当前帧源矩形供绘制使用）
  d->source =
      AnimationUpdate(&d->cat.animations[d->cat.playerAnimationState], dt);
}

static void TestDraw(GameScene *self) {
  TestData *d = (TestData *)self->data;

  // 相机启用时以场景相机视角绘制；禁用时直接以世界坐标绘制（固定视野）
  BeginSceneCamera(&d->sceneCamera);

  // 绘制平台
  DrawPlatform(&d->platform);
  DrawPlatform(&d->platform_m);

  // 绘制玩家
  DrawPlayer(&d->cat, d->source);

  // 绘制地面：位于世界坐标 y = logicHeight - 50，
  // 与 GroundCollision 中地面高度（480 - 50）保持一致
  DrawRectangle(0, d->app->logicHeight - 50, 1000.f, 50, LIGHTGRAY);

  EndSceneCamera(&d->sceneCamera);
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
  if (d->platform_m.platformTexture.id != 0) {
    UnloadTexture(d->platform_m.platformTexture);
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
