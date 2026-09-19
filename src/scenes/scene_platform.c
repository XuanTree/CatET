#include "game.h"
#include <raylib.h>
#include <raymath.h>
#include <stdlib.h>

// 我说C语言本质宏孩儿
#define PLATFORM_MAX_ENEMIES 16
#define PLATFORM_NUM_ENEMIES 6
#define PLATFORM_MAX_NUM 27
#define ENEMY_MAX_NUM 15
// ── 关卡布局：垂直爬塔 + 水平跳跃「随机混合」（跟随镜头，世界远超一屏）──────
// 每块平台相对上一块随机采用三种步型之一，既保留原「向上爬塔」，又加入横向
// 跳跃与向下绕行（攀爬 ~45% / 横跳 ~35% / 下落 ~20%）；每关随机朝左或朝右
// 推进，所有位移都在跳跃可达范围内。世界尺寸按实际占用推算，常达数屏宽/高，
// 由跟随镜头滚动展示，因此平台可大胆伸出屏幕之外。
// 全部布局数值集中在 core/game_config.h 的「平台关卡」段落，便于统一调参。

// 候选平台（中心 cx、spawn 左上 y、半宽 half）是否与已放置的 placedCount 个
// 平台重叠：矩形相交即视为重叠（y 方向按平台高度判定并额外留 ROW_GAP_MARGIN
// 净间隙）。主路线与备用路线的放置都经此校验，保证平台绝不重叠。
static bool PlatformPlacementBlocked(const Platform *plats, int placedCount,
                                     float cx, float spawnY, float half) {
  for (int i = 0; i < placedCount; i++) {
    const Platform *p = &plats[i];
    if (p->size.x <= 0.f)
      continue;
    if (fabsf(spawnY - p->spawnPosition.y) >= PLATFORM_BOX_H + ROW_GAP_MARGIN)
      continue; // y 方向分离足够，不可能重叠
    const float dx = fabsf(cx - (p->spawnPosition.x + p->size.x * 0.5f));
    if (dx < half + p->size.x * 0.5f + ROW_GAP_MARGIN)
      return true;
  }
  return false;
}

// 单步步型：攀爬（垂直为主）/ 横跳（水平为主）/ 下落（向右下绕行）
typedef enum PlatformStepKind {
  PLATFORM_STEP_CLIMB = 0,
  PLATFORM_STEP_RUN,
  PLATFORM_STEP_DROP,
} PlatformStepKind;

// 随机平台类型（随机步用 SMALL/MEDIUM；LARGE 半宽 192 更重更宽，作为「节奏
// 锚点」由生成循环每 PLATFORM_LARGE_EVERY 块显式安排，不参与随机）
static PlatformType RollPlatformType(void) {
  return (PlatformType)(SMALL + genRandomNum(MEDIUM - SMALL + 1));
}

// 随机步型（概率约 攀爬 45% / 横跳 35% / 下落 20%）
static PlatformStepKind RollStepKind(void) {
  const int roll = genRandomNum(100);
  if (roll < 45)
    return PLATFORM_STEP_CLIMB;
  if (roll < 80)
    return PLATFORM_STEP_RUN;
  return PLATFORM_STEP_DROP;
}

// 按步型生成水平位移 dx（正=向右）与垂直位移 dy（负=上升）
static void RollStepDelta(PlatformStepKind kind, float *dx, float *dy) {
  switch (kind) {
  case PLATFORM_STEP_CLIMB:
    *dx = (float)genRandomNum((int)(2.f * PLATFORM_CLIMB_DX) + 1) -
          PLATFORM_CLIMB_DX; // -120..120
    *dy = -(PLATFORM_CLIMB_RISE_MIN +
            (float)genRandomNum(
                (int)(PLATFORM_CLIMB_RISE_MAX - PLATFORM_CLIMB_RISE_MIN) +
                1)); // -90..-190
    break;
  case PLATFORM_STEP_RUN:
    *dx = PLATFORM_RUN_DX_MIN +
          (float)genRandomNum((int)(PLATFORM_RUN_DX_MAX - PLATFORM_RUN_DX_MIN) +
                              1);
    *dy = (float)genRandomNum((int)(2.f * PLATFORM_RUN_DY) + 1) -
          PLATFORM_RUN_DY; // -60..60
    break;
  default: // PLATFORM_STEP_DROP
    *dx = PLATFORM_DROP_DX_MIN +
          (float)genRandomNum(
              (int)(PLATFORM_DROP_DX_MAX - PLATFORM_DROP_DX_MIN) + 1);
    *dy = PLATFORM_DROP_DY_MIN +
          (float)genRandomNum(
              (int)(PLATFORM_DROP_DY_MAX - PLATFORM_DROP_DY_MIN) + 1);
    break;
  }
}

// 一次跳跃在「上升 rise（>=0）」时能覆盖的最大水平距离（取理论值的裕度），
// 与 player.c 物理一致：滞空时间 t2 = (v0 + sqrt(v0² + 2·g·rise)) / g，
// 水平可达距离 = 起跳水平速度 × t2。上升越高、水平可达越短；用于生成期
// 校验「这一步跳得过去」，避免出现理论上到不了的平台。
static float JumpReachX(float rise) {
  if (rise < 0.f)
    rise = 0.f; // 下落步：水平可达更长，按水平处理即可
  const float g = PLATFORM_GRAVITY;
  const float v0 = PLATFORM_JUMP_V0;
  const float disc = v0 * v0 + 2.f * g * rise;
  const float t2 = (v0 + sqrtf(disc)) / g;
  return PLATFORM_RUN_SPEED * t2 * PLATFORM_REACH_MARGIN;
}

// 平台世界半宽：platform_1/2/3.png 逻辑像素宽分别为 32/64/128，乘 GAME_SCALE 得
// 世界宽（SMALL=96、MEDIUM=192、LARGE=384）。布局阶段先按类型算尺寸（而不必
// 先初始化纹理），用于保证平台不重叠、不出界。
static float PlatformHalfWidth(PlatformType type) {
  float pxW = 32.f;
  if (type == MEDIUM)
    pxW = 64.f;
  else if (type == LARGE)
    pxW = 128.f;
  return pxW * 0.5f * GAME_SCALE;
}

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
  float worldWidth;   // 关卡世界右边界（可远超一屏，跟随镜头横向滚动）
  float worldLeft;    // 关卡世界左边界（朝左的关卡可为负）
  float worldTop;     // 关卡世界上边界（相机钳制/岩壁绘制用）
  float worldBottom;  // 关卡世界下边界（相机钳制/岩壁绘制用）
  float timeLimit;    // 本关限时（按实际路径长度推算，见 PlatformSceneEnter）
  Vector2 flagPos;    // 红旗位置（HUD 指南针用）
  int flagArrowIcon;  // 起点指示箭头图标（raygui ICON_ARROW_*）
  float startGroundW; // 起点矩形地面宽度（世界左端一屏宽的安全落脚地）
  // 检查点：每 PLATFORM_CHECKPOINT_EVERY 块平台设一个（坠落回到最近检查点，
  // 而不是最近落脚点，避免长关卡一次失误重跑全图）
  bool isCheckpoint[PLATFORM_MAX_NUM];
  Vector2 lastCheckpointPos; // 最近抵达的检查点位置（坠落重生点）
  int lastCheckpointIndex;   // 最近抵达的检查点平台下标（-1 表示仅在起点地面）
  int topPlatformIndex;      // 标记红旗生成的位置
  Flag flag;
  int level;
  int difficulty;
  float timeLeft;
  int lastTickSecond; // 最近一次 tick 的剩余整秒刻度（0=未提示，见 timer.h）
  Rectangle source;
  Rectangle enemySource[ENEMY_MAX_NUM]; // 敌怪当前动画帧源矩形
  bool enemyWasCountdown
      [ENEMY_MAX_NUM];       // 敌怪上一帧是否定格（检测触碰瞬间播放音效）
  bool clearEnemiesOnResume; // 战斗胜利返回后清空本关全部敌人（击败任意一个
                             // 敌人即清场，降低难度；见 PlatformSceneResume）

  // 剧情覆盖层（进入关卡时在出生点显示，逐字输出后保留原地，见 systems/story）
  StoryOverlay story;
  Vector2 storyAnchor; // 剧情文本框锚点（出生点，世界坐标）
} PlatformSceneData;

// 敌怪触碰定格 1s 后触发：经转场进入战斗场景（覆盖层），战斗胜利后 Pop 回到
// 本关卡。胜利返回时清空本关全部敌人（见 PlatformSceneResume）——即“击败
// 任意一个敌人即清场”，因此这里先标记 clearEnemiesOnResume 再压入战斗场景。
static void PlatformOnBattle(void *ctx) {
  PlatformBattleCtx *bc = (PlatformBattleCtx *)ctx;
  GameScene *self = bc->scene;
  PlatformSceneData *d = (PlatformSceneData *)self->data;
  Enemy *e = &d->enemies[bc->enemyIndex];
  // 胜利返回后清场（战斗失败时本场景会被 FailScene 替换，该标记无影响）
  d->clearEnemiesOnResume = true;
  GameStackPush(self->owner,
                TransitionSceneCreate(
                    d->app, BattleSceneCreate(d->app, &d->cat, e, self,
                                              d->level, d->difficulty)));
}

// 环境惩罚扣血（限时超时 / 坠落重生）统一入口：扣血、钳制下限，并同步
// lastHealth、原地触发 HIT 动画与受伤音效。与 UpdatePlayer 内置的“生命值
// 下降检测”不同——该检测会给玩家施加 -260 的受击上跳击退；超时/坠落本与
// 受击无关，若恰好发生在玩家起跳瞬间，会让玩家误以为“跳跃触发了扣血”
// （低概率复现的体感 BUG）。同步 lastHealth 后该检测不再触发，改为原地受
// 伤表现（与 scene_battle 的 BattleDamagePlayer 同款思路）。
static void PlatformDamagePlayer(PlatformSceneData *d, float amount) {
  Player *player = &d->cat;
  player->health -= amount;
  if (player->health < 0.f)
    player->health = 0.f;
  player->lastHealth =
      player->health;       // 同步基准，避免 UpdatePlayer 二次触发击退/音效
  PlayerTriggerHit(player); // 原地触发 HIT 动画（扣血仍有反馈）
  GameAppPlaySound(d->app, d->app->catHitSound, d->app->catHitSoundValid);
}

// 掉落重生（定义在下方，先声明供 PlatformSceneUpdate 使用）
static void RespawnIfFallen(PlatformSceneData *d, Player *player);

static void PlatformSceneEnter(GameScene *self) {
  PlatformSceneData *d = (PlatformSceneData *)self->data;
  // 关卡 BGM：平台/迷宫/拼写共用「Find The Letter.mp3」
  GameAppSetMusicTrack((GameApp *)d->app, MUSIC_TRACK_PLAY);
  // 避免内存未初始化导致的问题
  d->cat = (Player){0};
  InitPlayer(&d->cat);
  d->cat.app = d->app; // 注入音频宿主（受伤/跳跃音效）

  // 按难度应用最大生命值（Easy/Normal=100，Hard=125）
  PlayerApplyDifficulty(&d->cat, d->difficulty);

  // 继承上一关的生命值数据；新游戏（playerHealth=0）从满血开始
  if (d->app->playerHealth > 0.f) {
    d->cat.health = d->app->playerHealth;
  } else {
    d->cat.health = d->cat.maxHealth;
  }
  d->cat.lastHealth = d->cat.health;         // 同步受伤检测基准，避免进场误触发
  int baseCount = (d->difficulty == 0) ? 8 : // 简单
                      (d->difficulty == 1) ? 10   // 普通
                                           : 12;  // 困难
  d->platformCount = baseCount + (d->level / 10); // 随关卡推进平台数量增多
  if (d->platformCount > PLATFORM_MAX_PLATFORMS) {
    d->platformCount = PLATFORM_MAX_PLATFORMS;
  } // 不准平台数量超出上限

  // ── 平台链：从起点地面某一侧的保护带之外开始，逐块随机生成 ─────────────
  // 起点矩形地面本身就是出生落脚处，不再额外生成「出生平台」；且地面两侧
  // PLATFORM_SPAWN_CLEAR 距离内**不生成任何平台**，避免与地面视觉重合。
  // 每关随机选择「向右」或「向左」主推进方向，因此红旗也可能出现在左侧，
  // 不再清一色在右/上方；方向由起点地面上的指示箭头给出。
  d->startGroundW = (float)d->app->logicWidth;
  const float groundTop = (float)(d->app->logicHeight - 50);
  const float dirX = (genRandomNum(2) == 0) ? 1.f : -1.f; // 本关主推进方向
  // 保护带边界：向右关为「地面右缘 + CLEAR」，向左关为「地面左缘 - CLEAR」
  const float chainStartX = (dirX > 0.f)
                                ? (d->startGroundW + PLATFORM_SPAWN_CLEAR)
                                : (-PLATFORM_SPAWN_CLEAR);

  // 链起点锚点：以地面相应的边缘作为「上一块」的位置基准
  float prevCenter = (dirX > 0.f) ? d->startGroundW : 0.f;
  float prevSpawnY = groundTop;
  float prevHalf = 0.f;

  // 生成期世界边界：按平台数量 × 最大单步在主推进方向预留
  const float reach =
      chainStartX + dirX * (float)d->platformCount * PLATFORM_RUN_DX_MAX;
  const float limitMinX = (dirX > 0.f) ? chainStartX : reach;
  const float limitMaxX = (dirX > 0.f) ? reach : chainStartX;

  int i = 0;           // 从 0 开始：所有平台都属于链条（起点地面不是平台）
  float pathLen = 0.f; // 实际路径长度（相邻平台中心距之和，用于限时估算）
  while (i < d->platformCount) {
    PlatformType stepType = TOTAL_COUNT; // 哨兵：仍未放置成功
    float stepHalf = 0.f;
    float stepCenterX = 0.f;
    float stepSpawnY = 0.f;
    // 上一块是窄平台时限制本步水平距离（窄平台起跳助跑空间不足）
    const bool prevNarrow =
        (prevHalf > 0.f) && (prevHalf * 2.f <= PLATFORM_NARROW_WIDTH + 0.5f);
    // 每 PLATFORM_LARGE_EVERY 块安排一块 LARGE 大平台，作为节奏锚点
    const bool largeAnchor = (i > 0) && (i % PLATFORM_LARGE_EVERY == 0);

    // 最多尝试 6 次：每次随机步型/位移，与任何已放置平台重叠则重掷
    for (int attempt = 0; attempt < 6; attempt++) {
      const PlatformStepKind kind = RollStepKind();
      PlatformType type = largeAnchor ? LARGE : RollPlatformType();
      if (prevNarrow && type == LARGE)
        type = SMALL; // 窄平台后不放需要长间距的大平台
      const float half = PlatformHalfWidth(type);
      float dx, dy;
      RollStepDelta(kind, &dx, &dy);
      dx *= dirX; // 按本关主推进方向镜像水平位移（重力方向不变）

      // 可达性：上升越高、水平可达越短；窄平台助跑不足再缩放
      const float rise = (dy < 0.f) ? -dy : 0.f;
      float stepCap = JumpReachX(rise);
      if (prevNarrow)
        stepCap *= PLATFORM_NARROW_STEP_SCALE;

      const float signDx = (dx >= 0.f) ? 1.f : -1.f;
      float absDx = fabsf(dx);
      if (absDx > stepCap)
        absDx = stepCap; // 截到理论可达范围内
      if (kind != PLATFORM_STEP_CLIMB) {
        const float needDx = half + prevHalf + ROW_GAP_MARGIN;
        if (absDx < needDx)
          absDx = needDx; // 抬到不重叠所需的最小间距
      }
      if (absDx > stepCap + 0.5f)
        continue; // 最小间距已超出本步可达范围 → 换一步重掷
      dx = signDx * absDx;

      // 平台整体须落在起点保护带之外（侧别随主推进方向），并受世界边界钳制
      const float centerX =
          Clamp(prevCenter + dx, limitMinX + half, limitMaxX - half);
      const float spawnY =
          Clamp(prevSpawnY + dy, PLATFORM_PATH_TOP, PLATFORM_PATH_BOTTOM);
      if (!PlatformPlacementBlocked(d->platforms, i, centerX, spawnY, half)) {
        stepType = type;
        stepHalf = half;
        stepCenterX = centerX;
        stepSpawnY = spawnY;
        break;
      }
    }
    if (stepType == TOTAL_COUNT) {
      // 随机尝试均失败（极端拥挤）：退化为「确定可行」的保守步，不缩短关卡——
      // 先试正上方一小段垂直上升（垂直分离天然不重叠且一定可达），再试水平最小间距
      const float half = PlatformHalfWidth(SMALL);
      float centerX = prevCenter;
      float spawnY = Clamp(prevSpawnY - PLATFORM_CLIMB_RISE_MIN,
                           PLATFORM_PATH_TOP, PLATFORM_PATH_BOTTOM);
      if (PlatformPlacementBlocked(d->platforms, i, centerX, spawnY, half) ||
          centerX - half < limitMinX || centerX + half > limitMaxX) {
        centerX = Clamp(prevCenter + dirX * (half + prevHalf + ROW_GAP_MARGIN),
                        limitMinX + half, limitMaxX - half);
        spawnY = Clamp(prevSpawnY, PLATFORM_PATH_TOP, PLATFORM_PATH_BOTTOM);
      }
      if (PlatformPlacementBlocked(d->platforms, i, centerX, spawnY, half)) {
        d->platformCount = i; // 真正无解（空间耗尽）才收尾
        break;
      }
      stepType = SMALL;
      stepHalf = half;
      stepCenterX = centerX;
      stepSpawnY = spawnY;
    }

    d->platforms[i] = (Platform){0};
    InitJumpPlatforms(&d->platforms[i], (Vector2){0.f, 0.f}, stepType);
    d->platforms[i].spawnPosition.x = stepCenterX - stepHalf;
    d->platforms[i].spawnPosition.y = stepSpawnY;
    // 检查点标记：每 PLATFORM_CHECKPOINT_EVERY 块设一个
    d->isCheckpoint[i] = (i % PLATFORM_CHECKPOINT_EVERY == 0);
    // 累计实际路径长度（相邻落脚点中心距）
    pathLen += Vector2Distance((Vector2){stepCenterX, stepSpawnY},
                               (Vector2){prevCenter, prevSpawnY});
    i++;

    prevCenter = stepCenterX;
    prevSpawnY = stepSpawnY;
    prevHalf = stepHalf;
  }

  // 平台纹理按类型共享：生成期每块平台各自解码了一次同一张 PNG，这里合并为
  // 每种类型一份纹理（释放重复加载的，绘制与卸载都只操作这一份），
  // 显著减少解码次数与显存占用。
  {
    Texture2D typeTex[TOTAL_COUNT] = {0};
    for (int k = 0; k < d->platformCount; k++) {
      Platform *p = &d->platforms[k];
      if (p->platformTexture.id == 0)
        continue;
      const int t = (int)p->platformType;
      if (typeTex[t].id == 0) {
        typeTex[t] = p->platformTexture; // 该类型首次保留
      } else {
        UnloadTexture(p->platformTexture); // 重复加载的释放
        p->platformTexture = typeTex[t];   // 共享同一份纹理
      }
    }
  }

  // 世界边界按实际占用推算（含起点地面）：宽度 = 最右/最左平台边缘 + 边距；
  // 同时记录最高处（用于限时估算，垂直爬塔的爬升高度同样要计入）。
  float rightMost = d->startGroundW; // 至少包含起点地面
  float leftMost = 0.f;
  float topMost = groundTop; // 未爬升时以起点地面为基准
  for (int k = 0; k < d->platformCount; k++) {
    const float left = d->platforms[k].spawnPosition.x;
    const float right = left + d->platforms[k].size.x;
    if (right > rightMost)
      rightMost = right;
    if (left < leftMost)
      leftMost = left;
    if (d->platforms[k].spawnPosition.y < topMost)
      topMost = d->platforms[k].spawnPosition.y;
  }
  d->worldLeft = leftMost - PLATFORM_WORLD_MARGIN;
  d->worldWidth = rightMost + PLATFORM_WORLD_MARGIN;
  d->worldTop = topMost - PLATFORM_WORLD_MARGIN;
  d->worldBottom = PLATFORM_PATH_BOTTOM + PLATFORM_WORLD_MARGIN;
  // 本关限时：基础时长 + 实际路径长度 / 参考速度 × 裕度。比按包围盒估算更贴合
  // 真实耗时（垂直爬塔的爬升与横向跳跃的推进都计入）
  d->timeLimit =
      PLATFORM_TIME_BASE + pathLen / PLATFORM_TIME_SPEED * PLATFORM_TIME_MARGIN;

  // 玩家出生在起点地面上（起点地面已在上方创建/记录宽度）
  d->cat.position = (Vector2){100.f, groundTop - d->cat.size.y};
  d->cat.velocity = (Vector2){0.f, 0.f};
  d->cat.isOnTheGround = true;
  d->lastCheckpointPos = d->cat.position; // 未抵达检查点前，坠落回出生地面
  d->lastCheckpointIndex = -1;            // -1 表示仅在起点地面

  // ── 红旗：放在「距离出生点最远」的平台上 ──
  // 出生点 = 起点矩形地面（世界左端一屏宽，玩家实际站立处）的顶面中心；
  // 平台路线为「垂直爬塔 + 水平跳跃」随机混合，最远平台可能在高处或远端。
  const Vector2 spawnRef = {d->startGroundW * 0.5f,
                            (float)(d->app->logicHeight - 50)};
  d->topPlatformIndex = 0;
  float farthestDist2 = -1.f;
  for (int k = 0; k < d->platformCount; k++) {
    const Platform *p = &d->platforms[k];
    if (p->size.x <= 0.f)
      continue; // 跳过未初始化的空槽（防御）
    const float cx = p->spawnPosition.x + p->size.x * 0.5f;
    const float cy = p->spawnPosition.y + p->surfaceOffset;
    const float dx = cx - spawnRef.x;
    const float dy = cy - spawnRef.y;
    const float dist2 = dx * dx + dy * dy; // 平方距离比较（省去开方）
    if (dist2 > farthestDist2) {
      farthestDist2 = dist2;
      d->topPlatformIndex = k;
    }
  }
  Platform *top = &d->platforms[d->topPlatformIndex];
  const Vector2 flagPos = {top->spawnPosition.x + top->size.x * 0.5f,
                           top->spawnPosition.y + top->surfaceOffset};
  InitFlag(&d->flag, flagPos);
  d->flagPos = flagPos; // HUD 指南针基准

  // 起点指示箭头：给出本关红旗相对出生平台的方向（左 / 右 / 上），
  // 让玩家一开场就知道该往哪一侧推进（朝左关卡同样存在）。
  {
    const float sdx = flagPos.x - spawnRef.x;
    const float sdy = flagPos.y - spawnRef.y;
    // 垂直占绝对优势（> 1.5 倍水平）才用「上」箭头，否则给左右方向：
    // 平台链多为斜向推进，左右指示对玩家更有指导意义
    if (sdy < 0.f && fabsf(sdy) > 1.5f * fabsf(sdx))
      d->flagArrowIcon = ICON_ARROW_UP; // 红旗主要在正上方
    else
      d->flagArrowIcon = (sdx >= 0.f) ? ICON_ARROW_RIGHT : ICON_ARROW_LEFT;
  }

  // 按难度+关卡动态确定敌怪数量（上限 ENEMY_MAX_NUM，且不超过可用平台数）。
  // 降低难度：小幅度提高生成数量（2/3/4 起）。因击败任意一个敌人即清空
  // 全关敌人（见 PlatformSceneResume），敌人增多只是提供更多战斗机会与
  // 更多“一次清场”的收益，并不会叠加持续压力。
  int desired = (d->difficulty == 0) ? 2 : (d->difficulty == 1) ? 3 : 4;
  desired += d->level / 8; // 随关卡推进缓慢增多
  if (desired > ENEMY_MAX_NUM)
    desired = ENEMY_MAX_NUM;
  const int available = d->platformCount - 1; // 仅排除红旗平台（起点是地面）
  if (desired > available)
    desired = available;
  if (desired < 0)
    desired = 0;
  d->enemyCount = desired;

  // 敌人铺在平台链上（链本身就是必经路线）：按等间隔挑选，避免扎堆在同一段；
  // 等间隔天然保证下标互不相同，另排除红旗平台。
  for (int e = 0; e < d->enemyCount; e++) {
    int idx = ((e + 1) * d->platformCount) / (d->enemyCount + 1);
    if (idx >= d->platformCount)
      idx = d->platformCount - 1;
    if (idx == d->topPlatformIndex) // 避开红旗平台
      idx = (idx > 0) ? idx - 1 : idx + 1;
    d->enemyPlatformIndex[e] = idx;
    Platform *ep = &d->platforms[idx];
    d->enemies[e] = (Enemy){0};
    InitEnemy(&d->enemies[e], (Vector2){0.f, 0.f});
    // 站到平台可见顶面中心（顶面 y = spawnPosition.y + surfaceOffset）
    d->enemies[e].position.x =
        ep->spawnPosition.x + ep->size.x * 0.5f - d->enemies[e].size.x * 0.5f;
    d->enemies[e].position.y =
        ep->spawnPosition.y + ep->surfaceOffset - d->enemies[e].size.y;
    // 每个敌怪绑定独立的战斗回调上下文（记录触发战斗的敌怪下标）
    d->battleCtx[e] = (PlatformBattleCtx){self, e};
    d->enemies[e].onBattle = PlatformOnBattle;
    d->enemies[e].battleCtx = &d->battleCtx[e];
    d->enemySpots[e] = d->enemies[e].position; // 记录敌人落点
    d->enemySource[e] =
        AnimationUpdate(&d->enemies[e].animations[ENEMY_MOVE], 0.f);
  }

  // 初始化玩家动画源矩形（首帧）
  d->source =
      AnimationUpdate(&d->cat.animations[d->cat.playerAnimationState], 0.f);

  // 剧情：进入关卡时在出生点显示（打字机逐字输出，输出后保留原地；
  // 已完整通关一次后不再显示，见 systems/story）
  StoryOverlayStart(&d->story, d->app->isBeatGameOnce
                                   ? NULL
                                   : getStory(d->difficulty, d->level));
  d->storyAnchor = (Vector2){d->cat.position.x + d->cat.size.x * 0.5f,
                             d->cat.position.y - 8.0f};

  // 关卡限时：使用按世界规模推算出的本关限时，并复位警告刻度
  d->timeLeft = d->timeLimit;
  d->lastTickSecond = 0;

  // 初始化相机（保持原行为：硬跟随玩家居屏幕中心）
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

  // 右上角指南针：给出红旗相对玩家的方向与距离（大世界关卡尤为重要）
  HudDrawCompass(d->app, d->cat.position, d->flagPos);
}

static void PlatformSceneDraw(GameScene *self) {
  PlatformSceneData *d = (PlatformSceneData *)self->data;
  BeginSceneCamera(&d->sceneCamera);

  // 世界左右边界：岩壁（把「走到头」可视化，避免撞到看不见的墙）
  const Color rock = (Color){122, 118, 112, 255};
  DrawRectangle((int)(d->worldLeft - 80.f), (int)(d->worldTop - 400.f), 80,
                (int)(d->worldBottom - d->worldTop + 800.f), rock);
  DrawRectangle((int)d->worldWidth, (int)(d->worldTop - 400.f), 80,
                (int)(d->worldBottom - d->worldTop + 800.f), rock);

  for (int i = 0; i < d->platformCount; i++)
    DrawPlatform(&d->platforms[i]);
  DrawPlayer(&d->cat, d->source);
  for (int i = 0; i < d->enemyCount; i++)
    if (d->enemies[i].isAlive)
      DrawEnemy(&d->enemies[i], d->enemySource[i], 0.f);
  DrawFlag(&d->flag);

  // 起点矩形地面（世界左端一屏宽；与 GroundCollision 的碰撞面一致）
  DrawRectangle(0, d->app->logicHeight - 50, (int)d->startGroundW, 50,
                LIGHTGRAY);

  // 起点地面上的方向箭头：指示本关红旗在出生平台的哪一侧（左 / 右 / 上）
  GuiDrawIcon(d->flagArrowIcon, (int)(d->startGroundW * 0.5f) - 16,
              d->app->logicHeight - 50 + 8, 2, DARKGRAY);

  // 剧情文本框：锚定在世界坐标的出生点，不随玩家移动（世界可为负/远超一屏，
  // 故左右界用关卡世界边界而非屏幕宽度）
  StoryOverlayDrawAnchored(d->app, &d->story, d->storyAnchor.x,
                           d->storyAnchor.y, STORY_BOX_MAX_WIDTH,
                           STORY_BOX_FONT_SIZE, d->worldLeft, d->worldWidth);

  EndSceneCamera(&d->sceneCamera);

  // 全局 HUD：固定于逻辑屏幕坐标
  DrawHud(d);
}

static void PlatformSceneUpdate(GameScene *self, float dt) {
  PlatformSceneData *d = (PlatformSceneData *)self->data;

  // 剧情打字机：逐字输出（暂停时 dt=0 自动冻结）
  StoryOverlayUpdate(&d->story, dt);

  // 玩家生命值为 0 及以下时判定失败
  if (d->cat.health <= 0.f) {
    GameStackReplace(self->owner, FailSceneCreate(d->app));
    return;
  }

  // 触旗通关：经过渡场景进入下一关
  Rectangle playerRect = (Rectangle){d->cat.position.x, d->cat.position.y,
                                     d->cat.size.x, d->cat.size.y};
  if (FlagCheckCollision(&d->flag, playerRect)) {
    // 通关奖励：恢复生命值（随关卡递增，上限为最大生命值）
    PlayerHeal(&d->cat, ClearHealthReward(d->level));
    if (d->level >= MAX_LEVELS) {
      // 最终通关：记录速通最佳时间，经过渡进入通关结算场景
      // （scene_finish，最终胜利音效由该场景 onEnter 播放）
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

  // 关卡限时：倒计时归零扣血并重置（给玩家继续本关的机会）。环境惩罚统一走
  // PlatformDamagePlayer（原地 HIT，不做受击击退，避免与跳跃动作混淆）
  d->timeLeft -= dt;
  if (d->timeLeft <= 0.f) {
    PlatformDamagePlayer(d, TIME_PENALTY);
    d->timeLeft = d->timeLimit; // 重置为与关卡规模匹配的限时
  }
  // 剩余时间进入最后 COUNTDOWN_WARN_SECONDS 秒后，每跨一个整秒播放一次
  // tick 提示音（跨过 5/4/3/2/1 秒整各一声；归零/重置后自动重新武装）
  if (TimerCountdownWarn(&d->lastTickSecond, d->timeLeft,
                         COUNTDOWN_WARN_SECONDS))
    GameAppPlaySound(d->app, d->app->tickSound, d->app->tickSoundValid);

  // 触碰敌怪瞬间（isCountdown 上升沿）：播放 meet_the_enemy
  // 音效（画面定格开始）
  for (int i = 0; i < d->enemyCount; i++) {
    Enemy *e = &d->enemies[i];
    if (e->isAlive && e->isCountdown && !d->enemyWasCountdown[i]) {
      GameAppPlaySound(d->app, d->app->meetEnemySound,
                       d->app->meetEnemySoundValid);
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

  // 每帧先重置着地标记，再检测平台碰撞；起点区域另有一块矩形地面
  // （宽度 d->startGroundW），其余位置为虚空，坠落由 RespawnIfFallen 处理
  d->cat.isOnTheGround = false;
  for (int i = 0; i < d->platformCount; i++)
    PlayerCollision(&d->cat, &d->platforms[i]);
  GroundCollision(&d->cat, d->startGroundW);

  // 水平边界：世界左右两端之外为虚空，不允许走出世界（朝左关卡 worldLeft<0）
  if (d->cat.position.x < d->worldLeft)
    d->cat.position.x = d->worldLeft;
  if (d->cat.position.x + d->cat.size.x > d->worldWidth)
    d->cat.position.x = d->worldWidth - d->cat.size.x;

  // 检查点：站在检查点平台上即更新重生点并小幅回血（每个检查点只触发一次）
  if (d->cat.isOnTheGround) {
    for (int k = 0; k < d->platformCount; k++) {
      if (!d->isCheckpoint[k])
        continue;
      const Platform *cp = &d->platforms[k];
      if (cp->size.x <= 0.f)
        continue;
      const float top = cp->spawnPosition.y + cp->surfaceOffset;
      const float feet = d->cat.position.y + d->cat.size.y;
      const float cx = d->cat.position.x + d->cat.size.x * 0.5f;
      if (fabsf(feet - top) <= 3.f && cx >= cp->spawnPosition.x &&
          cx <= cp->spawnPosition.x + cp->size.x) {
        if (d->lastCheckpointIndex != k) {
          d->lastCheckpointIndex = k;
          d->lastCheckpointPos = d->cat.position;
          PlayerHeal(&d->cat, PLATFORM_CHECKPOINT_HEAL); // 检查点小幅回血
          GameAppPlaySound(d->app, d->app->uiSound, d->app->uiSoundValid);
        }
        break;
      }
    }
  }

  // 掉出可玩区域后回到最近检查点并扣血
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
    // 无地面：敌怪由 ePlatformCollision 站在所在平台上，且水平位置已钳制在
    // 该平台范围内，不会掉落

    Platform *ep = &d->platforms[d->enemyPlatformIndex[i]];
    float left = ep->spawnPosition.x;
    float right = left + ep->size.x - e->size.x;
    // 窄平台：巡逻振幅收窄到中段（SMALL 平台上敌怪会贴边来回抖动，观感差）
    if (ep->size.x < PLATFORM_NARROW_WIDTH * 1.6f) {
      const float span = right - left;
      left += span * 0.30f;
      right -= span * 0.30f;
      if (right < left)
        right = left;
    }
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
  // 平台纹理按类型共享（见 PlatformSceneEnter 的合并逻辑），故按纹理 id 去重后
  // 只卸载一次，避免同一张纹理被重复 Unload
  Texture2D unloaded[TOTAL_COUNT];
  int unloadedCount = 0;
  for (int i = 0; i < d->platformCount; i++) {
    const Texture2D t = d->platforms[i].platformTexture;
    if (t.id == 0)
      continue;
    bool dup = false;
    for (int k = 0; k < unloadedCount; k++) {
      if (unloaded[k].id == t.id) {
        dup = true;
        break;
      }
    }
    if (!dup && unloadedCount < TOTAL_COUNT) {
      UnloadTexture(t);
      unloaded[unloadedCount++] = t;
    }
  }
}

// 重新回到栈顶（战斗覆盖层弹出 / 暂停界面关闭）时：
//   - 恢复关卡 BGM：战斗场景会把 BGM 切到战斗曲，弹出后需切回关卡曲；
//     若当前已是关卡曲则 GameAppSetMusicTrack 内部直接忽略，不会重启曲目；
//   - 若刚打完一场胜利的战斗（PlatformOnBattle 标记）则清空本关全部敌人，
//     即“击败任意一个敌人即清场”，降低关卡难度。暂停界面关闭同样会触发
//     onResume，但那时 clearEnemiesOnResume 为 false，不会误清场。
static void PlatformSceneResume(GameScene *self) {
  PlatformSceneData *d = (PlatformSceneData *)self->data;
  GameAppSetMusicTrack((GameApp *)d->app, MUSIC_TRACK_PLAY);
  if (d->clearEnemiesOnResume) {
    d->clearEnemiesOnResume = false;
    for (int i = 0; i < d->enemyCount; i++)
      d->enemies[i].isAlive = false;
  }
}

// 坠落处理：y 超过路线最低点再下坠 PLATFORM_FALL_DEATH_Y 之外视为坠落，
// 退回「最近抵达的检查点」（长关卡避免一次失误重跑全图）并结算掉落惩罚。
static void RespawnIfFallen(PlatformSceneData *d, Player *player) {
  if (player->position.y <= PLATFORM_FALL_DEATH_Y)
    return; // 还在可玩区域内
  player->position = d->lastCheckpointPos;
  player->velocity = (Vector2){0.f, 0.f};
  player->isOnTheGround = true;
  PlatformDamagePlayer(d, player->maxHealth * FALL_PENALTY_RATIO); // 掉落惩罚
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
  scene->onResume = PlatformSceneResume; // 覆盖层弹出后恢复关卡 BGM

  return scene;
}