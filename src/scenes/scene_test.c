#include "game.h"

// ─────────────────────────────────────────────────────────────────────────────
// 测试用关卡，仅仅用来做关卡测试。0.1.0版本构建后，好像就没什么用了？？？
// 但是这个代码就不删除掉了,留作纪念吧
// 哦什么,多编译一个文件会使最后的包体积增大?
// 那咋了,那咋了,那咋了,我就留着
// 玩家在平台与地面之间跳跃前进，关卡终点设有小红旗，玩家触碰即通关，
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
  float timeLeft;        // 关卡限时倒计时（秒，玩法一由本场景暂代限时 40s）
  Rectangle source;      // 当前动画帧源矩形
  bool enemyWasCountdown; // 敌怪上一帧是否处于定格窗口（检测触碰瞬间播放音效）
} TestData;

// 玩家掉出屏幕外此距离后触发传送回最近的着陆表面
// （取接近一屏高度，避免玩家刚掉出底部一点就被传送，打断坠落体验）
#define FALL_RESPAWN_MARGIN 400.0f

// 关卡限时：玩法一（极速拼写）由本场景暂代，限时 40 秒
// （超时惩罚统一用 TIME_PENALTY，见 core/game_config.h）
#define TEST_TIME_LIMIT 40.0f

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
  // 本关有两个可站立平台（platform / platform_m）；数组大小必须与循环次数一致，
  // 否则循环越界读取 plats[i] 野指针导致崩溃（此前漏掉 platform_m 造成玩家
  // 掉出平台后崩溃）。
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

  // 掉落惩罚：每次掉出屏幕被传送回平台时，扣除最大生命值的 15%（下限 0）
  player->health -= player->maxHealth * FALL_PENALTY_RATIO;
  if (player->health < 0.0f)
    player->health = 0.0f;
}

// 敌怪触碰定格 1s 后触发：经转场进入战斗场景（覆盖层），战斗胜利后 Pop 回到
// 本关卡，并把被击败的敌怪标记为删除（isAlive=false）。
static void TestOnBattle(void *ctx) {
  GameScene *self = (GameScene *)ctx;
  TestData *d = (TestData *)self->data;
  GameStackPush(self->owner,
                TransitionSceneCreate(
                    d->app,
                    BattleSceneCreate(d->app, &d->cat, &d->enemy, self,
                                      d->level, d->difficulty)));
}

static void TestEnter(GameScene *self) {
  TestData *d = (TestData *)self->data;
  // 零初始化：杜绝未初始化内存导致的未定义行为。
  // 否则 platformTexture.id 等字段可能残留垃圾值
  d->cat = (Player){0};
  InitPlayer(&d->cat);
  d->cat.app = d->app; // 注入音频宿主（受伤/跳跃音效）
  // 按难度应用最大生命值（Easy/Normal=100，Hard=125）
  PlayerApplyDifficulty(&d->cat, d->difficulty);
  // 生命值继承：进入新关卡时恢复上一关剩余 HP（新游戏 playerHealth=0 → 满血）
  if (d->app->playerHealth > 0.0f) {
    d->cat.health = d->app->playerHealth;
  } else {
    d->cat.health = d->cat.maxHealth;
  }
  d->cat.lastHealth = d->cat.health; // 同步受伤检测基准，避免进场误触发
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

  // 关卡限时：玩法一（极速拼写）由本场景暂代，限时 40 秒
  d->timeLeft = TEST_TIME_LIMIT;

  // 隐式全局计时器：进入第一关开始计时（后续关卡保持累计）
  if (d->level == 1)
    SpeedrunStart((GameApp *)d->app);

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
    // 通关奖励：恢复生命值（随关卡递增，上限为最大生命值）
    PlayerHeal(&d->cat, ClearHealthReward(d->level));
    if (d->level >= MAX_LEVELS) {
      // 最终通关（第 MAX_LEVELS 关）：记录速通最佳时间，经过渡进入通关结算
      // 场景（scene_finish，最终胜利音效由该场景 onEnter 播放）
      SpeedrunFinish((GameApp *)d->app);
      GameStackReplace(self->owner, TransitionSceneCreate(
                                        d->app, FinishSceneCreate(d->app)));
    } else {
      // 普通通关：播放通关单关音效（scene_battle 不计入），经过渡进入下一关
      GameAppPlaySound(d->app, d->app->levelFinishSound,
                       d->app->levelFinishSoundValid);
      GameStackReplace(
          self->owner,
          TransitionSceneCreate(d->app, LevelFlowCreateNextScene(
                                            d->app, d->level, d->difficulty)));
    }
    return;
  }

  // 关卡限时（40 秒）：倒计时归零扣血并重置（给玩家继续本关的机会）
  d->timeLeft -= dt;
  if (d->timeLeft <= 0.0f) {
    d->cat.health -= TIME_PENALTY;
    if (d->cat.health < 0.0f)
      d->cat.health = 0.0f;
    d->timeLeft = TEST_TIME_LIMIT;
  }

  // 触碰敌怪瞬间（isCountdown 上升沿）：播放 meet_the_enemy
  // 音效（画面定格开始）
  if (d->enemy.isAlive && d->enemy.isCountdown && !d->enemyWasCountdown) {
    GameAppPlaySound(d->app, d->app->meetEnemySound,
                     d->app->meetEnemySoundValid);
  }
  d->enemyWasCountdown = d->enemy.isAlive && d->enemy.isCountdown;

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
  // 地面宽 1000：与 TestDraw 中 DrawRectangle(0, ..., 1000.f, 50) 一致
  GroundCollision(&d->cat, 1000.f);

  // 掉出屏幕外一定距离后传送到最近的平台/地面，避免无限下落
  RespawnIfFallen(d, &d->cat);

  // 敌怪更新：巡逻移动 + 重力 + 与地面/平台/玩家的碰撞
  if (d->enemy.isAlive) {
    d->enemy.isOnTheGround = false;
    UpdateEnemy(&d->enemy, dt);
    eGroundCollision(&d->enemy, 1000.f);
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
// 通用元素（关卡号/生命值条/时间/ESC）抽离到 tools/hud.h 供各场景复用。
static void DrawHud(TestData *d) {
  const int screenW = d->app->logicWidth;
  const float margin = 12.0f;
  const int fontSize = 16;

  // 左上角：当前关卡编号（全局 HUD）
  HudDrawLevel(d->app, d->level);

  // 顶部居中：通关提示（触碰红旗）
  // 字号取 16 = UI_FONT_BASE_SIZE(48)/3 的整数倍：像素字点采样在整数倍
  // 缩放下每行像素完整对齐；非整数倍（如 14px）会跳过字形下半部分像素，
  // 导致提示文字“只能看到上半部分、下半部分丢失”。
  const char *goalHint = "Reach the red flag to clear this level";
  GameAppDrawText(d->app, goalHint,
                  (screenW - GameAppMeasureText(d->app, goalHint, 16)) / 2,
                  (int)(margin + fontSize + 4), 16, GRAY);

  // 左下角：生命值条（全局 HUD，传入继承后的当前 HP）
  HudDrawHealthBar(d->app, d->cat.health, d->cat.maxHealth);

  // 右下角：关卡限时倒计时（40 秒，mm:ss）
  HudDrawTime(d->app, d->timeLeft);

  // 右上角：ESC 暂停提示（全局 HUD）
  HudDrawEscHint(d->app);
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
    DrawEnemy(&d->enemy, d->enemySource, 0.f);

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
