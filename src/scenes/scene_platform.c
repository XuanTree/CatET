#include "game.h"
#include <raylib.h>
#include <raymath.h>
#include <stdlib.h>

// 我说C语言本质宏孩儿
#define PLATFORM_MAX_ENEMIES 16
#define PLATFORM_NUM_ENEMIES 6
#define PLATFORM_MAX_NUM 27
#define ENEMY_MAX_NUM 15
#define TOWER_TOP_MARGIN 200.f      // 塔顶预留高度
#define MAX_RISE_SAFE 190.f         // 安全值,玩家跳不上去那不是完蛋了?
#define MAX_SIDE_OFFSET 125.f       // 水平偏移数据
#define MIN_SIDE_OVERLAP 40.f       // 相邻平台水平区间必须重叠的余量;
#define PLATFORM_TIME_LIMIT 40.0f   // 关卡限时 40 秒
#define PLATFORM_TIME_PENALTY 20.0f // 倒计时归零扣血

// 敌怪战斗回调上下文：记录触发战斗的敌怪下标（多敌怪场景需要知道删除谁），
// 由每个敌怪的 battleCtx 指向本场景数据中对应的元素。
typedef struct PlatformBattleCtx {
  GameScene *scene; // 平台关卡场景（供回调经 self->owner 切换）
  int enemyIndex;   // 触发战斗的敌怪下标
} PlatformBattleCtx;

typedef struct PlatformSceneData {
  const GameApp *app;
  Player cat;                   // 玩家
  SceneCamera sceneCamera;      // 跟随镜头
  Enemy enemies[ENEMY_MAX_NUM]; // 敌怪数组（数量随难度/关卡动态变化）
  int enemyCount;
  int enemyPlatformIndex[ENEMY_MAX_NUM]; // 敌怪所在平台下标（巡逻边界钳制用）
  PlatformBattleCtx battleCtx[ENEMY_MAX_NUM]; // 每个敌怪的战斗回调上下文
  Vector2 enemySpots[PLATFORM_MAX_ENEMIES];
  Platform firstPlatform;
  Platform platforms[PLATFORM_MAX_NUM];
  int platformCount;
  int topPlatformIndex; // 标记红旗生成的位置
  Flag flag;
  int level;
  int difficulty;
  float timeLeft;
  Rectangle source;
  Rectangle enemySource[ENEMY_MAX_NUM]; // 敌怪当前动画帧源矩形
  bool enemyWasCountdown
      [ENEMY_MAX_NUM]; // 敌怪上一帧是否定格（检测触碰瞬间播放音效）
} PlatformSceneData;

// 敌怪触碰定格 1s 后触发：经转场进入战斗场景（覆盖层），战斗胜利后 Pop 回到
// 本关卡，并把被击败的敌怪标记为删除（isAlive=false）。
static void PlatformOnBattle(void *ctx) {
  PlatformBattleCtx *bc = (PlatformBattleCtx *)ctx;
  GameScene *self = bc->scene;
  PlatformSceneData *d = (PlatformSceneData *)self->data;
  Enemy *e = &d->enemies[bc->enemyIndex];
  GameStackPush(
      self->owner,
      TransitionSceneCreate(
          d->app, BattleSceneCreate(d->app, &d->cat, e, self, d->difficulty)));
}

// 掉落重生（定义在下方，先声明供 PlatformSceneUpdate 使用）
static void RespawnIfFallen(PlatformSceneData *d, Player *player);

static void PlatformSceneEnter(GameScene *self) {
  PlatformSceneData *d = (PlatformSceneData *)self->data;
  // 避免内存未初始化导致的问题
  d->cat = (Player){0};
  InitPlayer(&d->cat);
  d->cat.app = d->app; // 注入音频宿主（受伤/跳跃音效）

  // 继承上一关的生命值数据
  if (d->app->playerHealth > 0.f) {
    d->cat.health = d->app->playerHealth;
  }
  d->cat.lastHealth = d->cat.health;         // 同步受伤检测基准，避免进场误触发
  int baseCount = (d->difficulty == 0) ? 8 : // 简单
                      (d->difficulty == 1) ? 12  // 普通
                                           : 16; // 困难
  d->platformCount = baseCount + (d->level / 5); // 随关卡推进平台数量增多
  if (d->platformCount > PLATFORM_MAX_NUM) {
    d->platformCount = PLATFORM_MAX_NUM;
  } // 不准平台数量超出上限

  // 场景布局
  const float groundTop = (float)(d->app->logicHeight - 50);
  const float worldWidth = (float)d->app->logicWidth;

  // 出生点平台
  d->platforms[0] = (Platform){0};
  InitJumpPlatforms(&d->platforms[0],
                    (Vector2){worldWidth * 0.5f - 60.f, groundTop - 90.f},
                    SMALL);

  Vector2 prevTop = (Vector2){
      .x = d->platforms[0].spawnPosition.x + d->platforms[0].size.x * 0.5f,
      .y = d->platforms[0].spawnPosition.y + d->platforms[0].surfaceOffset};

  for (int i = 1; i < d->platformCount; i++) {
    float rise = 80.f + genRandomNum((int)(MAX_RISE_SAFE - 80));
    // 上升越高，水平偏移越收紧（保证跳得上去）
    float maxDx =
        MAX_SIDE_OFFSET * (1.f - (rise - 80.f) / (MAX_RISE_SAFE - 80.f));
    float dx = (genRandomNum(2) ? 1.f : -1.f) * genRandomNum((int)maxDx);

    float cx = prevTop.x + dx;
    float cy = prevTop.y - rise; // 越往上 y 越小
    // 边界钳制：左右不能出世界
    if (cx < 60.f)
      cx = 60.f + genRandomNum(80);
    if (cx > worldWidth - 60.f)
      cx = worldWidth - 60.f - genRandomNum(80);

    // 爬塔模式只用小/中平台（Large 太宽，破坏爬塔节奏）
    PlatformType type =
        (PlatformType)(SMALL + genRandomNum(MEDIUM - SMALL + 1));
    d->platforms[i] = (Platform){0};
    InitJumpPlatforms(&d->platforms[i], (Vector2){0.f, 0.f}, type); // 临时位置
    d->platforms[i].spawnPosition.x =
        cx - d->platforms[i].size.x * 0.5f; // 中心→左上
    d->platforms[i].spawnPosition.y = cy;

    float prevL = d->platforms[i - 1].spawnPosition.x;
    float prevR = prevL + d->platforms[i - 1].size.x;
    if (d->platforms[i].spawnPosition.x + d->platforms[i].size.x <
        prevL + MIN_SIDE_OVERLAP)
      d->platforms[i].spawnPosition.x =
          prevL + MIN_SIDE_OVERLAP - d->platforms[i].size.x;
    else if (d->platforms[i].spawnPosition.x > prevR - MIN_SIDE_OVERLAP)
      d->platforms[i].spawnPosition.x = prevR - MIN_SIDE_OVERLAP;

    prevTop = (Vector2){
        .x = d->platforms[i].spawnPosition.x + d->platforms[i].size.x * 0.5f,
        .y = d->platforms[i].spawnPosition.y + d->platforms[i].surfaceOffset,
    };
  }

  // 生成红旗
  d->topPlatformIndex = d->platformCount - 1;
  Platform *top = &d->platforms[d->topPlatformIndex];
  InitFlag(&d->flag, (Vector2){top->spawnPosition.x + top->size.x * 0.5f,
                               top->spawnPosition.y + top->surfaceOffset});

  // 按难度+关卡动态确定敌怪数量（上限 ENEMY_MAX_NUM，且不超过可用平台数）
  int desired = (d->difficulty == 0) ? 1 : (d->difficulty == 1) ? 2 : 3;
  desired += d->level / 8; // 随关卡推进缓慢增多
  if (desired > ENEMY_MAX_NUM)
    desired = ENEMY_MAX_NUM;
  int available = d->platformCount - 2; // 排除出生平台与红旗平台
  if (desired > available)
    desired = available;
  if (desired < 0)
    desired = 0;
  d->enemyCount = desired;

  // 随机选互不相同的平台放敌人（排除出生平台与红旗平台）
  for (int i = 0; i < d->enemyCount; i++) {
    int idx;
    bool dup;
    do {
      idx = 1 + genRandomNum(d->platformCount - 2);
      dup = false;
      for (int k = 0; k < i; k++) {
        if (d->enemyPlatformIndex[k] == idx) {
          dup = true;
          break;
        }
      }
    } while (dup);
    d->enemyPlatformIndex[i] = idx;
    Platform *ep = &d->platforms[idx];
    d->enemies[i] = (Enemy){0};
    InitEnemy(&d->enemies[i], (Vector2){0.f, 0.f});
    // 站到平台可见顶面中心（顶面 y = spawnPosition.y + surfaceOffset）
    d->enemies[i].position.x =
        ep->spawnPosition.x + ep->size.x * 0.5f - d->enemies[i].size.x * 0.5f;
    d->enemies[i].position.y =
        ep->spawnPosition.y + ep->surfaceOffset - d->enemies[i].size.y;
    // 每个敌怪绑定独立的战斗回调上下文（记录触发战斗的敌怪下标）
    d->battleCtx[i] = (PlatformBattleCtx){self, i};
    d->enemies[i].onBattle = PlatformOnBattle;
    d->enemies[i].battleCtx = &d->battleCtx[i];
    d->enemySpots[i] = d->enemies[i].position; // 记录敌人落点
    d->enemySource[i] =
        AnimationUpdate(&d->enemies[i].animations[ENEMY_MOVE], 0.f);
  }

  // 初始化玩家动画源矩形（首帧）
  d->source =
      AnimationUpdate(&d->cat.animations[d->cat.playerAnimationState], 0.f);

  // 关卡限时：进入关卡重置倒计时
  d->timeLeft = PLATFORM_TIME_LIMIT;

  // 初始化相机
  InitSceneCamera(&d->sceneCamera, d->app->logicWidth, d->app->logicHeight,
                  true, CAMERA_FOLLOW_CENTER);
  if (d->level == 1) {
    SpeedrunStart((GameApp *)d->app);
  }
}

// 关卡全局 HUD：左上角关卡号、顶部通关提示、左下角生命值条、右下角剩余时间、
// 右上角 ESC 提示。在场景相机之外绘制，固定于逻辑屏幕坐标。
static void DrawHud(PlatformSceneData *d) {
  const int screenW = d->app->logicWidth;
  const float margin = 12.0f;
  const int fontSize = 16;

  // 左上角：当前关卡编号
  HudDrawLevel(d->app, d->level);

  // 顶部居中：通关提示（触碰红旗）
  const char *goalHint = "Reach the red flag to clear this level";
  GameAppDrawText(d->app, goalHint,
                  (screenW - GameAppMeasureText(d->app, goalHint, 16)) / 2,
                  (int)(margin + fontSize + 4), 16, GRAY);

  // 左下角：生命值条
  HudDrawHealthBar(d->app, d->cat.health, d->cat.maxHealth);

  // 右下角：关卡限时倒计时
  HudDrawTime(d->app, d->timeLeft);

  // 右上角：ESC 暂停提示
  HudDrawEscHint(d->app);
}

static void PlatformSceneDraw(GameScene *self) {
  PlatformSceneData *d = (PlatformSceneData *)self->data;
  BeginSceneCamera(&d->sceneCamera);

  for (int i = 0; i < d->platformCount; i++)
    DrawPlatform(&d->platforms[i]);
  DrawPlayer(&d->cat, d->source);
  for (int i = 0; i < d->enemyCount; i++)
    if (d->enemies[i].isAlive)
      DrawEnemy(&d->enemies[i], d->enemySource[i]);
  DrawFlag(&d->flag);
  DrawRectangle(0, d->app->logicHeight - 50, d->app->logicWidth, 50, LIGHTGRAY);

  EndSceneCamera(&d->sceneCamera);

  // 全局 HUD：固定于逻辑屏幕坐标
  DrawHud(d);
}

static void PlatformSceneUpdate(GameScene *self, float dt) {
  PlatformSceneData *d = (PlatformSceneData *)self->data;

  // 玩家生命值为 0 及以下时判定失败
  if (d->cat.health <= 0.f) {
    GameStackReplace(self->owner, FailSceneCreate(d->app));
    return;
  }

  // 触旗通关：经过渡场景进入下一关
  Rectangle playerRect = (Rectangle){d->cat.position.x, d->cat.position.y,
                                     d->cat.size.x, d->cat.size.y};
  if (FlagCheckCollision(&d->flag, playerRect)) {
    if (d->level >= MAX_LEVELS) {
      // 最终通关：播放最终胜利音效，记录速通最佳时间，经过渡回到开始菜单
      if (d->app->gameFinishSoundValid)
        PlaySound(d->app->gameFinishSound);
      SpeedrunFinish((GameApp *)d->app);
      GameStackReplace(
          self->owner,
          TransitionSceneCreate(d->app, StartSceneCreate((GameApp *)d->app)));
    } else {
      // 普通通关：播放通关单关音效（scene_battle 不计入），经过渡进入下一关
      if (d->app->levelFinishSoundValid)
        PlaySound(d->app->levelFinishSound);
      GameStackReplace(
          self->owner,
          TransitionSceneCreate(d->app, LevelFlowCreateNextScene(
                                            d->app, d->level, d->difficulty)));
    }
    return;
  }

  // 关卡限时：倒计时归零扣血并重置（给玩家继续本关的机会）
  d->timeLeft -= dt;
  if (d->timeLeft <= 0.f) {
    d->cat.health -= PLATFORM_TIME_PENALTY;
    if (d->cat.health < 0.f)
      d->cat.health = 0.f;
    d->timeLeft = PLATFORM_TIME_LIMIT;
  }

  // 触碰敌怪瞬间（isCountdown 上升沿）：播放 meet_the_enemy
  // 音效（画面定格开始）
  for (int i = 0; i < d->enemyCount; i++) {
    Enemy *e = &d->enemies[i];
    if (e->isAlive && e->isCountdown && !d->enemyWasCountdown[i]) {
      if (d->app->meetEnemySoundValid)
        PlaySound(d->app->meetEnemySound);
    }
    d->enemyWasCountdown[i] = e->isAlive && e->isCountdown;
  }

  // 敌怪定格窗口：任一敌怪定格则冻结画面，仅推进各自战斗计时
  bool anyCountdown = false;
  for (int i = 0; i < d->enemyCount; i++) {
    if (d->enemies[i].isAlive && d->enemies[i].isCountdown) {
      anyCountdown = true;
      break;
    }
  }
  if (anyCountdown) {
    for (int i = 0; i < d->enemyCount; i++)
      if (d->enemies[i].isAlive)
        ePlayerCollision(&d->enemies[i], &d->cat);
    return;
  }

  UpdatePlayer(&d->cat, dt);

  // 每帧先重置着地标记，再检测平台/地面碰撞
  d->cat.isOnTheGround = false;
  for (int i = 0; i < d->platformCount; i++)
    PlayerCollision(&d->cat, &d->platforms[i]);
  GroundCollision(&d->cat);

  // 掉出底部后回出生平台
  RespawnIfFallen(d, &d->cat);

  // 敌怪：巡逻 + 站平台 + 玩家碰撞（各自限界在所在平台内，防止走出掉落）
  for (int i = 0; i < d->enemyCount; i++) {
    Enemy *e = &d->enemies[i];
    if (!e->isAlive)
      continue;
    e->isOnTheGround = false;
    UpdateEnemy(e, dt);
    for (int j = 0; j < d->platformCount; j++)
      ePlatformCollision(e, &d->platforms[j]);
    eGroundCollision(e);

    Platform *ep = &d->platforms[d->enemyPlatformIndex[i]];
    const float left = ep->spawnPosition.x;
    const float right = left + ep->size.x - e->size.x;
    if (e->position.x < left) {
      e->position.x = left;
      e->isHaveGoneRight = true;
    } else if (e->position.x > right) {
      e->position.x = right;
      e->isHaveGoneRight = false;
    }

    ePlayerCollision(e, &d->cat);
    d->enemySource[i] = AnimationUpdate(&e->animations[ENEMY_MOVE], dt);
  }

  // 相机跟随玩家 + 更新动画
  SetCameraTarget(&d->sceneCamera, d->cat.position);
  UpdateSceneCamera(&d->sceneCamera, dt);
  d->source =
      AnimationUpdate(&d->cat.animations[d->cat.playerAnimationState], dt);
}

static void PlatformSceneExit(GameScene *self) {
  PlatformSceneData *d = (PlatformSceneData *)self->data;
  ((GameApp *)d->app)->playerHealth = d->cat.health; // 供下一关继承

  UnloadTexture(d->cat.idleTexture);
  UnloadTexture(d->cat.runTexture);
  UnloadTexture(d->cat.jumpTexture);
  UnloadTexture(d->cat.sleepTexture);
  UnloadTexture(d->cat.hitTexture);
  for (int i = 0; i < d->enemyCount; i++)
    if (d->enemies[i].idleTexture.id != 0)
      UnloadTexture(d->enemies[i].idleTexture);
  for (int i = 0; i < d->platformCount; i++)
    if (d->platforms[i].platformTexture.id != 0)
      UnloadTexture(d->platforms[i].platformTexture);
}

static void RespawnIfFallen(PlatformSceneData *d, Player *player) {
  const float groundTop = (float)(d->app->logicHeight - 50);
  if (player->position.y <= groundTop + 200.f)
    return; // 还没掉到底
  player->position =
      (Vector2){d->app->logicWidth * 0.5f - player->size.x * 0.5f,
                groundTop - player->size.y};
  player->velocity = (Vector2){0.f, 0.f};
  player->isOnTheGround = true;
  player->health -= player->maxHealth * 0.2f; // 掉落惩罚
  if (player->health < 0.f)
    player->health = 0.f;
}

GameScene *PlatformSceneCreate(const GameApp *app, int difficulty, int level) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  if (scene == NULL) {
    return NULL;
  }
  PlatformSceneData *data =
      (PlatformSceneData *)calloc(1, sizeof(PlatformSceneData));
  if (data == NULL) {
    free(scene);
    return NULL;
  }
  // 基本关卡信息初始化
  data->app = app;
  data->level = level;
  data->difficulty = difficulty;

  scene->name = "PlatformScene";
  scene->data = data;
  scene->flags = GAME_SCENE_DRAW_WHEN_HIDDEN; // 暂停时仍作为背景绘制
  scene->pauseable = true;                    // 允许暂停

  scene->onEnter = PlatformSceneEnter;
  scene->onUpdate = PlatformSceneUpdate;
  scene->onDraw = PlatformSceneDraw;
  scene->onExit = PlatformSceneExit;

  return scene;
}