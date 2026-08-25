#include "scenes/scene_test.h"
#include "entities/enemy.h"
#include "entities/flag.h"
#include "entities/platform.h"
#include "entities/player.h"
#include "raylib.h"
#include "scenes/scene_fail.h"
#include "scenes/scene_transition.h"
#include "systems/level_flow.h"
#include "tools/camera.h"
#include "tools/raygui.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// ─────────────────────────────────────────────────────────────────────────────
// 平台跳跃关卡（docs/game_instructions.md 关卡设计 3，核心玩法）：
//   - 玩家在平台与地面之间跳跃前进，无显式倒计时。
//   - 关卡终点设有小红旗，玩家触碰即通关，进入下一关（关卡类型按权重刷新）。
//   - HP 归零判定失败。
// ─────────────────────────────────────────────────────────────────────────────

// 场景私有数据：栈持有并负责释放
typedef struct TestData {
  const GameApp *app; // 只读引用，不拥有
  Player cat;
  SceneCamera sceneCamera; // 场景相机：由本场景持有并决定启用/禁用
  Platform platform;
  Platform platform_m;
  Enemy enemy;           // 平台上的敌怪：触碰后 1s 定格进入战斗场景
  Rectangle enemySource; // 敌怪当前动画帧源矩形
  Flag flag;             // 终点小红旗：触碰即通关
  int level;             // 当前关卡编号（创建时注入，通关后经 level_flow 推进）
  int difficulty;        // 难度（0/1/2，传递给后续关卡）
  Rectangle source;      // 当前动画帧源矩形
} TestData;

// 玩家掉出屏幕外此距离后触发传送回最近的着陆表面
// （取接近一屏高度，避免玩家刚掉出底部一点就被传送，打断坠落体验）
#define FALL_RESPAWN_MARGIN 400.0f

// 可站立着陆表面：地面矩形 + 各平台顶面（水平中心 + 顶面 y）
typedef struct LandingSurface {
  float centerX; // 表面水平中心（世界坐标）
  float topY;    // 表面顶面 y（玩家脚可直接站立）
} LandingSurface;

// 玩家矩形（世界坐标，绘制与碰撞统一）
static Rectangle PlayerRect(const Player *p) {
  return (Rectangle){p->position.x, p->position.y, p->size.x, p->size.y};
}

// 玩家掉出屏幕外一定距离后，传送到水平距离最近的平台（或地面矩形）顶面上，
// 避免无限下落导致玩家完全脱离关卡。
static void RespawnIfFallen(TestData *d, Player *player) {
  const float fallThreshold = (float)d->app->logicHeight + FALL_RESPAWN_MARGIN;
  if (player->position.y <= fallThreshold)
    return;

  // 收集所有可着陆表面：地面矩形（与绘制一致）+ 各平台顶面
  LandingSurface surfaces[3];
  int count = 0;
  surfaces[count++] = (LandingSurface){
      .centerX = 500.0f, // 地面矩形 (0, logicHeight - 50) 宽 1000
      .topY = (float)(d->app->logicHeight - 50),
  };
  Platform *plats[] = {&d->platform, &d->platform_m};
  for (int i = 0; i < 2; i++) {
    if (plats[i]->size.x <= 0.0f || plats[i]->size.y <= 0.0f)
      continue; // 加载失败（尺寸无效）的平台不参与
    surfaces[count++] = (LandingSurface){
        .centerX = plats[i]->spawnPosition.x + plats[i]->size.x * 0.5f,
        .topY = plats[i]->spawnPosition.y + plats[i]->surfaceOffset,
    };
  }

  // 选择水平距离玩家最近的表面
  int best = 0;
  float bestDist = fabsf(surfaces[0].centerX - player->position.x);
  for (int i = 1; i < count; i++) {
    const float dist = fabsf(surfaces[i].centerX - player->position.x);
    if (dist < bestDist) {
      bestDist = dist;
      best = i;
    }
  }

  // 传送到最近表面顶面之上（水平居中，避免落在边缘再次掉落）
  player->position.x = surfaces[best].centerX - player->size.x * 0.5f;
  player->position.y = surfaces[best].topY - player->size.y;
  player->velocity = (Vector2){0.0f, 0.0f};
  player->isOnTheGround = true;

  // 掉落惩罚：每次掉出屏幕被传送回平台时，扣除最大生命值的 20%（下限 0）
  player->health -= player->maxHealth * 0.2f;
  if (player->health < 0.0f)
    player->health = 0.0f;
}

// 敌怪触碰定格 1s 后触发：进入战斗场景。
// 占位：战斗场景未实现，此处不做任何事——玩家碰到敌怪不扣血（触碰不产生
// 伤害），平台场景原有血量机制（掉落扣血等）保持不变。
// TODO: BattleSceneCreate 实现后替换为：
//   GameStackPush(self->owner, BattleSceneCreate(d->app, &d->cat, &d->enemy,
//   self));
static void TestOnBattle(void *ctx) {
  GameScene *self = (GameScene *)ctx;
  (void)self;
}

static void TestEnter(GameScene *self) {
  TestData *d = (TestData *)self->data;
  // 零初始化：杜绝未初始化内存导致的未定义行为。
  // 否则 platformTexture.id 等字段可能残留垃圾值
  d->cat = (Player){0};
  InitPlayer(&d->cat);
  // 生命值继承：进入新关卡时恢复上一关剩余 HP（新游戏 playerHealth=0 → 满血）
  if (d->app->playerHealth > 0.0f)
    d->cat.health = d->app->playerHealth;
  d->platform = (Platform){0};
  InitJumpPlatforms(&d->platform, (Vector2){100, 350}, SMALL);
  d->platform_m = (Platform){0};
  InitJumpPlatforms(&d->platform_m, (Vector2){300, 200}, MEDIUM);

  // 终点小红旗：立于地面顶面靠右位置（地面顶面 y = logicHeight - 50）
  const float groundTop = (float)(d->app->logicHeight - 50);
  InitFlag(&d->flag, (Vector2){900.0f, groundTop});

  // 敌怪：出生在玩家与红旗之间的地面，掉落后站在地面巡逻，成为真实障碍；
  // 触碰后 1s 定格窗口结束触发战斗回调（战斗场景未实现，占位）。
  d->enemy = (Enemy){0};
  InitEnemy(&d->enemy, (Vector2){450, 200});
  d->enemy.onBattle = TestOnBattle;
  d->enemy.battleCtx = self;

  // 初始化场景相机
  InitSceneCamera(&d->sceneCamera, d->app->logicWidth, d->app->logicHeight,
                  true, CAMERA_FOLLOW_CENTER);
}

static void TestUpdate(GameScene *self, float dt) {
  TestData *d = (TestData *)self->data;

  // 玩家生命值为 0 及以下时判定失败：替换为失败场景，结束当前关卡。
  // 失败界面只提供「回到菜单」/「退出游戏」两个选择（见 scene_fail.c）。
  if (d->cat.health <= 0.0f) {
    GameStackReplace(self->owner, FailSceneCreate(d->app));
    return;
  }

  // 触碰终点小红旗 → 通关：经过渡场景进入下一关（类型按权重刷新）
  if (FlagCheckCollision(&d->flag, PlayerRect(&d->cat))) {
    GameStackReplace(
        self->owner,
        TransitionSceneCreate(
            d->app, LevelFlowCreateNextScene(d->app, d->level, d->difficulty)));
    return;
  }

  // 触碰提醒窗口：画面定格（玩家与整个世界不更新，玩家不可移动），
  // 仅推进敌怪战斗延迟计时；满 1s 后 ePlayerCollision 触发 onBattle。
  if (d->enemy.isAlive && d->enemy.isCountdown) {
    ePlayerCollision(&d->enemy, &d->cat);
    return;
  }

  UpdatePlayer(&d->cat, dt);

  // 每帧先重置着地标记，再检测平台/地面碰撞。
  // 若不重置，离开平台边缘后 isOnTheGround 仍为 true，玩家会悬空。
  d->cat.isOnTheGround = false;
  PlayerCollision(&d->cat, &d->platform);
  PlayerCollision(&d->cat, &d->platform_m);
  GroundCollision(&d->cat);

  // 掉出屏幕外一定距离后传送到最近的平台/地面，避免无限下落
  RespawnIfFallen(d, &d->cat);

  // 敌怪更新：巡逻移动 + 重力 + 与地面/平台/玩家的碰撞
  if (d->enemy.isAlive) {
    d->enemy.isOnTheGround = false;
    UpdateEnemy(&d->enemy, dt);
    eGroundCollision(&d->enemy);
    ePlatformCollision(&d->enemy, &d->platform);
    ePlatformCollision(&d->enemy, &d->platform_m);
    ePlayerCollision(&d->enemy, &d->cat);
    d->enemySource = AnimationUpdate(&d->enemy.animations[ENEMY_MOVE], dt);
  }

  // 每帧把玩家位置设为相机跟随目标，再按场景配置的模式更新相机。
  // 若场景禁用相机（DisableSceneCamera），此处不会更新，保持固定视野。
  SetCameraTarget(&d->sceneCamera, d->cat.position);
  UpdateSceneCamera(&d->sceneCamera, dt);

  // 更新动画（记录当前帧源矩形供绘制使用）
  d->source =
      AnimationUpdate(&d->cat.animations[d->cat.playerAnimationState], dt);
}

// 关卡全局 HUD：左上角关卡号、顶部通关提示、左下角生命值条、
// 右下角游戏时间、右上角 ESC 提示。在场景相机之外绘制，固定于逻辑屏幕坐标。
static void DrawHud(TestData *d) {
  const float margin = 12.0f;
  const int fontSize = 16;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 左上角：当前关卡编号
  char levelText[24];
  snprintf(levelText, sizeof(levelText), "Level : %d", d->level);
  GameAppDrawText(d->app, levelText, (int)margin, (int)margin, fontSize,
                  DARKGRAY);

  // 顶部居中：通关提示（触碰红旗）
  // 字号取 16 = UI_FONT_BASE_SIZE(48)/3 的整数倍：像素字点采样在整数倍
  // 缩放下每行像素完整对齐；非整数倍（如 14px）会跳过字形下半部分像素，
  // 导致提示文字“只能看到上半部分、下半部分丢失”。
  const char *goalHint = "Reach the red flag to clear this level";
  GameAppDrawText(d->app, goalHint,
                  (screenW - GameAppMeasureText(d->app, goalHint, 16)) / 2,
                  (int)(margin + fontSize + 4), 16, GRAY);

  // 左下角：生命值可视化进度条（颜色随剩余血量变化）。
  // bar 起点预留左侧 "HP" 标签空间，避免 raygui 左侧文本绘制到屏幕外。
  const float barW = 150.0f;
  const float barH = 16.0f;
  const float labelW = 24.0f; // "HP" 标签宽 + 间距
  const float barX = margin + labelW;
  const float barY = (float)screenH - margin - barH;
  const Rectangle hpBounds = {
      .x = barX, .y = barY, .width = barW, .height = barH};

  char hpText[16];
  snprintf(hpText, sizeof(hpText), "%d/%d", (int)d->cat.health,
           (int)d->cat.maxHealth);
  float hpValue = d->cat.health;
  const float hpRatio =
      (d->cat.maxHealth > 0.0f) ? d->cat.health / d->cat.maxHealth : 0.0f;
  const int prevTextSize = GuiGetStyle(DEFAULT, TEXT_SIZE);
  const int prevBarColor = GuiGetStyle(PROGRESSBAR, BASE_COLOR_PRESSED);
  // 血量 >50% 绿色，25%~50% 橙色，<=25% 红色
  if (hpRatio <= 0.25f) {
    GuiSetStyle(PROGRESSBAR, BASE_COLOR_PRESSED, 0xe74c3cff);
  } else if (hpRatio <= 0.50f) {
    GuiSetStyle(PROGRESSBAR, BASE_COLOR_PRESSED, 0xe67e22ff);
  } else {
    GuiSetStyle(PROGRESSBAR, BASE_COLOR_PRESSED, 0x27ae60ff);
  }
  GuiSetStyle(DEFAULT, TEXT_SIZE, fontSize);
  GuiProgressBar(hpBounds, "HP", hpText, &hpValue, 0.0f, d->cat.maxHealth);
  GuiSetStyle(DEFAULT, TEXT_SIZE, prevTextSize);
  GuiSetStyle(PROGRESSBAR, BASE_COLOR_PRESSED, prevBarColor);

  // 右下角：当前游戏时间（mm:ss，来自全局计时器 runTime，右对齐）
  const int totalSec = (int)d->app->runTime;
  char timeText[32];
  snprintf(timeText, sizeof(timeText), "Time %02d:%02d", totalSec / 60,
           totalSec % 60);
  const int timeW = GameAppMeasureText(d->app, timeText, fontSize);
  GameAppDrawText(d->app, timeText, screenW - (int)margin - timeW,
                  screenH - (int)margin - fontSize, fontSize, DARKGRAY);

  // 右上角：方框内含 ESC 提示（提示玩家按 ESC 暂停）
  const char *escText = "ESC";
  const int escW = GameAppMeasureText(d->app, escText, fontSize);
  const float boxW = (float)escW + 20.0f;
  const float boxH = (float)fontSize + 12.0f;
  const float boxX = (float)screenW - margin - boxW;
  const float boxY = margin;
  DrawRectangle((int)boxX, (int)boxY, (int)boxW, (int)boxH, Fade(BLACK, 0.55f));
  DrawRectangleLines((int)boxX, (int)boxY, (int)boxW, (int)boxH, DARKGRAY);
  GameAppDrawText(d->app, escText, (int)(boxX + (boxW - (float)escW) * 0.5f),
                  (int)(boxY + (boxH - (float)fontSize) * 0.5f), fontSize,
                  WHITE);
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

  // 绘制敌怪（战斗胜利后 isAlive=false 不再绘制，即“删除该敌怪”）
  if (d->enemy.isAlive)
    DrawEnemy(&d->enemy, d->enemySource);

  // 绘制终点小红旗
  DrawFlag(&d->flag);

  // 绘制地面：位于世界坐标 y = logicHeight - 50，
  // 与 GroundCollision 中地面高度（480 - 50）保持一致
  DrawRectangle(0, d->app->logicHeight - 50, 1000.f, 50, LIGHTGRAY);

  EndSceneCamera(&d->sceneCamera);

  // 全局 HUD：固定于逻辑屏幕坐标（左上角生命值+时间，右上角暂停提示）
  DrawHud(d);
}

static void TestExit(GameScene *self) {
  TestData *d = (TestData *)self->data;
  // 保存当前 HP 供下一关继承（失败/回菜单时由开始场景重置为 0）
  ((GameApp *)d->app)->playerHealth = d->cat.health;
  // 卸载本场景加载的资源（与场景生命周期绑定）
  UnloadTexture(d->cat.idleTexture);
  UnloadTexture(d->cat.runTexture);
  UnloadTexture(d->cat.jumpTexture);
  UnloadTexture(d->cat.sleepTexture);
  UnloadTexture(d->enemy.idleTexture);
  if (d->platform.platformTexture.id != 0) {
    UnloadTexture(d->platform.platformTexture);
  }
  if (d->platform_m.platformTexture.id != 0) {
    UnloadTexture(d->platform_m.platformTexture);
  }
}

GameScene *TestSceneCreate(const GameApp *app, int level, int difficulty) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  if (scene == NULL)
    return NULL;
  TestData *data = (TestData *)calloc(1, sizeof(TestData));
  if (data == NULL) {
    free(scene);
    return NULL;
  }
  data->app = app;
  data->level = level;
  data->difficulty = difficulty;

  scene->name = "TestScene";
  scene->data = data;
  scene->flags = GAME_SCENE_DRAW_WHEN_HIDDEN; // 暂停时仍绘制关卡作为背景
  scene->pauseable = true;                    // 关卡内允许按 ESC 调出暂停界面
  scene->onEnter = TestEnter;
  scene->onUpdate = TestUpdate;
  scene->onDraw = TestDraw;
  scene->onExit = TestExit;
  // onPause / onResume 暂不需要，保持 NULL
  return scene;
}
