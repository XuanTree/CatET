#include "scenes/scene_bossfight.h"
#include "game.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// 高难度关卡 Boss 战（docs/game_instructions.md 关卡设计 4，固定每 20
// 关刷新）：
//   - 关卡类型 1（极速拼写）与 3（平台跳跃）的结合，锁定镜头。
//   - 中心大平台 + 多个小平台；boss 在屏幕上方区域左右往返飞行，周期性释放
//     弹幕（BulletPatternFire 随机 pattern），玩家需躲避弹幕（弹幕命中扣血）。
//   - 屏幕上方掉落字母供玩家拼写；拼对一个单词（填满全部挖空）才扣除 boss
//     生命值 20%（整词在隐藏暴击窗口内完成则扣除 25%），填对一个字母不扣血；
//     拼写错误仅困难难度扣血（简单/普通无惩罚），拼写后无论对错都重置为
//     下一个拼写。
//   - boss 生命值归零 → 胜利进入下一关（第 MAX_LEVELS 关则通关回菜单）；
//     玩家 HP 归零 → 失败。
// ─────────────────────────────────────────────────────────────────────────────

// ── 关卡常量 ──────────────────────────────────────────────────────────────
#define BOSSFIGHT_MAX_BULLETS 24        // 弹幕数组上限
#define BOSSFIGHT_MAX_SMALL_PLATFORMS 4 // 小平台数量（中心大平台 + 多个小平台）
#define BOSSFIGHT_MAX_FALL_LETTERS 8    // 空中下落字母数组上限
#define BOSSFIGHT_INVINCIBLE_TIME                                              \
  1.5f                             // 弹幕命中后无敌时间（秒，与战斗场景一致）
#define BOSS_HP_MAX 100            // boss 生命值（每正确拼写扣 20%）
#define BOSS_HP_DMG_NORMAL 20      // 固定扣除 20%（docs）
#define BOSS_HP_DMG_CRIT 25        // 快速拼写暴击扣除 25%（docs）
#define BOSS_CRIT_WINDOW 6.0f      // 隐藏暴击窗口（秒）：窗口内拼写正确 → 暴击
#define BOSS_SHOOT_INTERVAL 2.2f   // 弹幕发射间隔（秒）
#define BOSS_AFK_DURATION 1.2f     // 开场备战缓冲（秒）：boss 暂不攻击
#define BOSS_PATROL_SPEED 120.f    // boss 巡逻速度（世界坐标/秒）
#define BOSS_PATROL_Y 48.f         // boss 巡逻高度（屏幕上方）
#define BOSS_EDGE_MARGIN 24.f      // boss 巡逻左右边界距屏幕边缘
#define BOSS_VOLLEY_BASE 4         // 弹幕数量下限（每难度 +2）
#define BOSS_VOLLEY_RANDOM_EXTRA 2 // 弹幕数量随机增量
#define BOSS_BULLET_SPEED 230.f    // 弹幕飞行速度（世界坐标/秒）
// 困难难度拼写错误扣血用 game_config 的 BOSS_WRONG_PENALTY_HARD（=30）
#define BOSS_LARGE_TOP 380.f  // 中心大平台可见顶面 y（世界坐标）
#define BOSS_FALL_SPEED 150.f // 字母下落速度（世界坐标/秒）
#define BOSS_DISTRACTORS 2    // 每批干扰字母数（与剩余正确字母一同掉落）
#define BOSS_PLAT_PAD 20.f    // 字母出生/落点距平台边缘的最小间距

// 天上落下的字母：下落中 → 落地后并入 character.letters（由 Character 组件
// 负责拾取/拼写判定），merged 标记避免重复绘制/重复并入。
typedef struct BossFightFallingLetter {
  char ch;        // 字母字符
  bool isCorrect; // 是否为所需正确字母
  Vector2 pos;    // 圆心位置（世界坐标）
  bool falling;   // 是否仍在空中下落
  bool merged;    // 是否已并入 character.letters
} BossFightFallingLetter;

typedef struct BossFightSceneData {
  const GameApp *app;
  // Entities
  Boss *boss;
  Bullet *bullets;
  Player *cat;
  Platform large_platform;
  Character character;
  Rectangle rec;
  SceneCamera
      camera; // 锁定镜头（禁用相机，固定视野：世界坐标 == 逻辑屏幕坐标）
  // States
  bool isBossDead;
  // Timer
  Timer boss_afkTimer;
  Timer boss_shootTimer;
  // Data
  float timeLeft;
  int bulletCount;
  int difficulty;
  int level;

  // 场景运行状态
  GameStack *owner;         // 所属栈（供拼写/失败事件切换场景）
  bool transitionRequested; // 已请求场景切换，防止同帧重复切换
  int bossDir;              // boss 巡逻方向（1=右，-1=左）
  bool bossCanShoot;        // 开场备战是否结束（boss 开始攻击）
  float afkGrace;           // 开场备战计时（dt 驱动，暂停即冻结）
  float shootTimer;         // 弹幕发射间隔计时（dt 驱动）
  float wrongPenalty;       // 拼写错误扣血（按难度预计算，仅困难 > 0）
  Rectangle bossSource;     // boss 当前动画帧源矩形
  Platform small_platforms[BOSSFIGHT_MAX_SMALL_PLATFORMS];
  int smallPlatformCount;
  BossFightFallingLetter falling[BOSSFIGHT_MAX_FALL_LETTERS];
  int fallingCount;
} BossFightSceneData;

// ── 前向声明 ──────────────────────────────────────────────────────────────
// 供 BossFightSceneEnter 注入 Character 组件的回调（定义在本文件下方）。
static Vector2 BossFightDropResolver(void *ctx, const Player *p);
static void BossFightOnSpellCorrect(void *ctx);
static void BossFightOnSpellWrong(void *ctx);

// ── 工具函数
// ──────────────────────────────────────────────────────────────────

// 平台可见顶面 y（世界坐标）：贴图左上角 + 顶部透明留白
static float BossFightPlatformTop(const Platform *p) {
  return p->spawnPosition.y + p->surfaceOffset;
}

// 随机干扰字母：必须不在单词中（既非剩余挖空字母，也非已填/可见字母），
// 避免玩家误以为某个「单词已含字母」也是待填项而误判为正确。
static char BossFightRandomDistractor(const BossFightSceneData *d) {
  char ch;
  do {
    ch = (char)('a' + genRandomNum(26));
  } while (strchr(d->character.entry.word, ch) != NULL);
  return ch;
}

// 随机选一个平台（中心大平台权重更高：拼写发生在中心大平台，字母更常落其上）
static Platform *BossFightRandomPlatform(BossFightSceneData *d) {
  const int total = 2 + d->smallPlatformCount; // 大平台占 2 份权重
  const int idx = genRandomNum(total);
  if (idx < 2)
    return &d->large_platform;
  return &d->small_platforms[idx - 2];
}

// 掉落一批字母：包含「当前仍未填写的全部挖空字母」（去重）作为正确候选 +
// BOSS_DISTRACTORS 个干扰字母。玩家正确选择其中任一挖空字母后，清空平台并
// 掉落下一批（见 BossFightOnSpellCorrect）；全部挖空填满后重置为下一个单词。
// 出生 x 限定在随机平台的水平范围内，保证字母必然落到某个平台上可拾取。
static void BossFightSpawnWave(BossFightSceneData *d) {
  // 收集仍未填写的挖空字母（去重）
  char answers[CHARACTER_MAX_BLANKS];
  int answerCount = 0;
  for (int i = 0; i < d->character.blankCount; i++) {
    int idx = d->character.blankIndex[i];
    if (idx < 0 || d->character.revealed[idx] != '_')
      continue; // 已填好的挖空不再掉落
    char ch = d->character.entry.word[idx];
    bool dup = false;
    for (int k = 0; k < answerCount; k++)
      if (answers[k] == ch) {
        dup = true;
        break;
      }
    if (!dup)
      answers[answerCount++] = ch;
  }
  if (answerCount == 0)
    return; // 无剩余挖空（防御，正常应已重置单词）

  // 本批字母：全部剩余正确字母 + 干扰字母（防空位）
  int count = answerCount + BOSS_DISTRACTORS;
  if (count > BOSSFIGHT_MAX_FALL_LETTERS - d->fallingCount)
    count = BOSSFIGHT_MAX_FALL_LETTERS - d->fallingCount;
  if (count < 1)
    return;

  for (int i = 0; i < count; i++) {
    Platform *p = BossFightRandomPlatform(d);
    const float minX = p->spawnPosition.x + BOSS_PLAT_PAD;
    const float maxX = p->spawnPosition.x + p->size.x - BOSS_PLAT_PAD;
    const int rangeX = (int)(maxX - minX);
    const int guardX = rangeX > 0 ? rangeX : 1; // 除零/负区间防护

    BossFightFallingLetter *f = &d->falling[d->fallingCount++];
    f->isCorrect = (i < answerCount);
    f->ch = f->isCorrect ? answers[i] : BossFightRandomDistractor(d);
    f->pos = (Vector2){minX + (float)genRandomNum(guardX + 1), -30.0f};
    f->falling = true;
    f->merged = false;
  }
}

// 字母落地后并入 Character 组件：letterCount 满时移除一个未被拾取的旧字母
// 为新字母腾出位置（避免平台上字母无限堆积）。
static void BossFightMergeLanded(BossFightSceneData *d,
                                 BossFightFallingLetter *f) {
  if (f->falling)
    return; // 仍在下落：不可并入
  Character *c = &d->character;
  if (c->letterCount >= CHARACTER_MAX_LETTERS) {
    for (int i = 0; i < c->letterCount; i++) {
      if (!c->letters[i].isPickedUp) {
        c->letters[i] = c->letters[c->letterCount - 1];
        c->letterCount--;
        break;
      }
    }
  }
  if (c->letterCount < CHARACTER_MAX_LETTERS) {
    c->letters[c->letterCount++] =
        (CharLetter){f->ch, f->isCorrect, false, f->pos};
    f->merged = true;
  }
}

// 下落字母更新：下落 → 撞到任一平台顶面落地并入 character；掉出屏幕底部则
// 移除（防御）；已落地但平台满未并入的每帧重试并入。
static void BossFightUpdateFalling(BossFightSceneData *d, float dt) {
  const float r = d->character.pickupRadius;
  const int screenH = d->app->logicHeight;
  // 收集全部平台（中心大平台 + 小平台）
  Platform *plats[BOSSFIGHT_MAX_SMALL_PLATFORMS + 1];
  int n = 0;
  plats[n++] = &d->large_platform;
  for (int i = 0; i < d->smallPlatformCount; i++)
    plats[n++] = &d->small_platforms[i];

  for (int i = 0; i < d->fallingCount; i++) {
    BossFightFallingLetter *f = &d->falling[i];
    if (f->merged)
      continue;
    if (f->falling) {
      f->pos.y += BOSS_FALL_SPEED * dt;
      // 找字母水平范围内、底缘已到达其顶面且最高的平台（即最先到达的落点）；
      // 40px 容差防止字母穿过平台间隙后误落到下方更远的平台。
      float bestTop = -1.0f;
      for (int k = 0; k < n; k++) {
        Platform *p = plats[k];
        if (p->size.x <= 0.0f)
          continue;
        if (f->pos.x < p->spawnPosition.x ||
            f->pos.x > p->spawnPosition.x + p->size.x)
          continue;
        const float top = BossFightPlatformTop(p);
        if (f->pos.y + r >= top && f->pos.y <= top + 40.0f) {
          if (bestTop < 0.0f || top < bestTop)
            bestTop = top;
        }
      }
      if (bestTop >= 0.0f) {
        f->pos.y = bestTop - r;
        f->falling = false;
      } else if (f->pos.y > (float)screenH + 60.0f) {
        // 掉出屏幕底部：移除（用末尾元素填充，避免数组空洞）
        d->falling[i] = d->falling[d->fallingCount - 1];
        d->fallingCount--;
        i--;
        continue;
      }
    }
    // 已落地（含刚落地 / 平台满未并入）：尝试并入 character
    BossFightMergeLanded(d, f);
  }
}

// boss 发射弹幕：随机 pattern + 数量随难度递增，目标指向玩家
static void BossFightFireVolley(BossFightSceneData *d) {
  const Boss *boss = d->boss;
  const Player *player = d->cat;
  const int lo =
      BOSS_VOLLEY_BASE + 2 * d->difficulty; // easy 4 / normal 6 / hard 8
  const int hi = lo + BOSS_VOLLEY_RANDOM_EXTRA;
  const int count = lo + genRandomNum(hi - lo + 1);
  const Vector2 origin = {boss->position.x + boss->size.x * 0.5f,
                          boss->position.y + boss->size.y * 0.5f};
  const Vector2 target = {player->position.x + player->size.x * 0.5f,
                          player->position.y + player->size.y * 0.5f};
  d->bulletCount =
      BulletPatternFire(d->bullets, BOSSFIGHT_MAX_BULLETS, BulletPatternRoll(),
                        origin, target, count, BOSS_BULLET_SPEED, 0.f);
  // 浮动伤害：每颗弹幕随机 3~9 点伤害（docs 难度弹幕 +100% 改为固定浮动区间）
  for (int i = 0; i < d->bulletCount; i++)
    d->bullets[i].damage = BulletRollDamage();
}

// boss 更新：动画推进 + 左右往返巡逻 + 周期性发射弹幕
// （boss 不受重力，可自由飞行；原 UpdateBoss 的“恒定向右”示例逻辑不适用，
//   故由本场景接管移动，实现往返巡逻）
static void BossFightUpdateBoss(BossFightSceneData *d, float dt) {
  Boss *boss = d->boss;
  const int screenW = d->app->logicWidth;
  if (d->isBossDead)
    return;

  // 动画推进（boss.png 8 帧循环），记录当前帧源矩形
  d->bossSource = AnimationUpdate(&boss->animations[BOSS_MOVE], dt);

  // 巡逻：左右往返。right 为 boss 最左缘允许到达的最大值（使其右缘不越过
  // 屏幕右边界减边距）；比较用 position.x 而非 position.x+size.x，否则双重
  // 扣除 size.x 会导致回钳后条件恒成立、boss 永久卡在右上角不动。
  boss->position.x += BOSS_PATROL_SPEED * (float)d->bossDir * dt;
  const float left = BOSS_EDGE_MARGIN;
  const float right = (float)screenW - BOSS_EDGE_MARGIN - boss->size.x;
  if (boss->position.x < left) {
    boss->position.x = left;
    d->bossDir = 1;
  } else if (boss->position.x > right) {
    boss->position.x = right;
    d->bossDir = -1;
  }

  // 开场备战：boss 先不攻击（给玩家熟悉拼写的时间）
  if (!d->bossCanShoot) {
    d->afkGrace += dt;
    if (d->afkGrace >= BOSS_AFK_DURATION) {
      d->bossCanShoot = true;
      d->shootTimer = 0.0f;
    }
    return;
  }

  // 周期性发射弹幕
  d->shootTimer += dt;
  if (d->shootTimer >= BOSS_SHOOT_INTERVAL) {
    d->shootTimer = 0.0f;
    BossFightFireVolley(d);
  }
}

// 玩家物理：平台跳跃 + 与全部平台碰撞 + 屏幕边界钳制 + 掉出底部送回大平台
static void BossFightUpdatePlayer(BossFightSceneData *d, float dt) {
  Player *player = d->cat;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;
  UpdatePlayer(player, dt);
  player->isOnTheGround = false;
  PlayerCollision(player, &d->large_platform);
  for (int i = 0; i < d->smallPlatformCount; i++)
    PlayerCollision(player, &d->small_platforms[i]);
  // 水平边界钳制在屏幕内
  if (player->position.x < 0.0f)
    player->position.x = 0.0f;
  if (player->position.x + player->size.x > (float)screenW)
    player->position.x = (float)screenW - player->size.x;
  // 掉出屏幕底部 → 送回中心大平台顶面（防御）
  if (player->position.y > (float)screenH + 100.0f) {
    player->position =
        (Vector2){d->large_platform.spawnPosition.x +
                      d->large_platform.size.x * 0.5f - player->size.x * 0.5f,
                  BossFightPlatformTop(&d->large_platform) - player->size.y};
    player->velocity = (Vector2){0, 0};
    player->isOnTheGround = true;
  }
}

// 弹幕更新：位移 + 飞出屏幕失效 + 命中玩家扣血并销毁
static void BossFightUpdateBullets(BossFightSceneData *d, float dt) {
  Player *player = d->cat;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;
  for (int i = 0; i < d->bulletCount; i++) {
    Bullet *b = &d->bullets[i];
    if (!b->isActive)
      continue;
    UpdateBullet(b, dt);
    // 飞出屏幕外 → 失效（docs：弹幕消失包括飞出屏幕外）
    if (b->position.x + b->size.x < 0.0f || b->position.x > (float)screenW ||
        b->position.y + b->size.y < 0.0f || b->position.y > (float)screenH) {
      b->isActive = false;
      continue;
    }
    // 命中玩家 → 扣血并销毁
    const Rectangle pr = {player->position.x, player->position.y,
                          player->size.x, player->size.y};
    const Rectangle br = {b->position.x, b->position.y, b->size.x, b->size.y};
    if (CheckCollisionRecs(pr, br)) {
      // 无敌期间：弹幕穿身而过，不扣血、不销毁（命中后短暂免伤窗口，
      // 避免多颗弹幕在连续帧内多次命中造成连续扣血）
      if (player->invincibleTimer > 0.0f)
        continue;
      player->health -= b->damage;
      if (player->health < 0.0f)
        player->health = 0.0f;
      player->invincibleTimer = BOSSFIGHT_INVINCIBLE_TIME;
      // 致命一击：本帧扣血到 0 后，下一帧帧首会提前切换到失败场景，等不到
      // UpdatePlayer 的受伤检测，故立即播放受伤音效；非致命由下一帧
      // UpdatePlayer 统一触发（避免同一命中重复播放两次音效）
      if (player->health <= 0.0f) {
        GameAppPlaySound(d->app, d->app->catHitSound, d->app->catHitSoundValid);
      }
      b->isActive = false;
      continue;
    }
  }
}

// 重置拼写：生成新谜题、清空平台字母与空中字母、重新掉落一波、重置隐藏
// 暴击窗口。拼写正确/错误都走此重置（docs：单词无论拼写正确错误，都重置
// 让玩家进行下一步拼写）。
static void BossFightResetPuzzle(BossFightSceneData *d) {
  CharacterSetupPuzzle(&d->character);
  d->character.letterCount = 0;
  d->character.holdingLetter = false;
  d->character.heldLetterIndex = -1;
  for (int i = 0; i < CHARACTER_MAX_LETTERS; i++)
    d->character.letters[i].isPickedUp = false;
  d->fallingCount = 0;
  d->timeLeft = BOSS_CRIT_WINDOW; // 重置隐藏暴击窗口
  BossFightSpawnWave(d);
}

// 胜利：最终胜利（第 MAX_LEVELS 关）或普通通关进入下一关（经转场，只请求一次）
static void BossFightAdvanceNext(BossFightSceneData *d) {
  if (d->transitionRequested)
    return;
  d->transitionRequested = true;
  d->isBossDead = true;
  // 通关 boss 关卡奖励：恢复生命值（随关卡递增，上限为最大生命值）
  PlayerHeal(d->cat, BossClearHealthReward(d->level));
  if (d->level >= MAX_LEVELS) {
    // 最终通关（第 MAX_LEVELS 关）：记录速通最佳时间，经过渡进入通关结算
    // 场景（scene_finish，最终胜利音效由该场景 onEnter 播放）
    SpeedrunFinish((GameApp *)d->app);
    GameStackReplace(d->owner,
                     TransitionSceneCreate(d->app, FinishSceneCreate(d->app)));
    return;
  }
  // 普通通关：播放通关单关音效，经过渡进入下一关
  GameAppPlaySound(d->app, d->app->levelFinishSound,
                   d->app->levelFinishSoundValid);
  GameStackReplace(d->owner, TransitionSceneCreate(
                                 d->app, LevelFlowCreateNextScene(
                                             d->app, d->level, d->difficulty)));
}

// ── Character 组件回调
// ─────────────────────────────────────────────────────────

// 拼写正确：只有拼对一个单词（全部挖空填满）才扣除 boss 生命值（隐藏暴击
// 窗口内完成则暴击扣 25%）；填对一个字母但尚未拼完整词时不扣血，仅清空平台
// 并掉落下一批继续拼写同一单词。boss 生命值归零 → 胜利，否则重置为下一个单词。
static void BossFightOnSpellCorrect(void *ctx) {
  BossFightSceneData *d = (BossFightSceneData *)ctx;
  if (CharacterRemainingBlanks(&d->character) > 0) {
    // 还有挖空（整词未拼完）：不扣 boss 血，只清空平台上其余字母并掉落下一
    // 批；暴击窗口继续计时（整词完成才算一次正确拼写）
    d->character.letterCount = 0;
    d->character.holdingLetter = false;
    d->character.heldLetterIndex = -1;
    d->fallingCount = 0;
    BossFightSpawnWave(d);
    return;
  }
  // 整词拼写完成：扣除 boss 生命值（整词在隐藏暴击窗口内完成则暴击）
  const int dmg = (d->timeLeft > 0.0f) ? BOSS_HP_DMG_CRIT : BOSS_HP_DMG_NORMAL;
  d->boss->hp -= dmg;
  if (d->boss->hp <= 0) {
    d->boss->hp = 0;
    BossFightAdvanceNext(d);
    return;
  }
  // 生成下一个单词（重置拼写）
  BossFightResetPuzzle(d);
}

// 拼写错误：仅困难难度扣血（简单/普通无拼写错误扣血惩罚，docs 关卡设计 4），
// 无论对错都重置为下一个拼写。
static void BossFightOnSpellWrong(void *ctx) {
  BossFightSceneData *d = (BossFightSceneData *)ctx;
  d->cat->health -= d->wrongPenalty;
  if (d->cat->health < 0.0f)
    d->cat->health = 0.0f;
  BossFightResetPuzzle(d);
}

// 放下落点解析（Character 回调）：默认放回中心大平台顶面（水平按玩家中心
// 钳制，保证任意位置放下都不悬空、可再拾取）
static Vector2 BossFightDropResolver(void *ctx, const Player *p) {
  BossFightSceneData *d = (BossFightSceneData *)ctx;
  float x = p->position.x + p->size.x * 0.5f;
  const float left = d->large_platform.spawnPosition.x;
  const float right =
      d->large_platform.spawnPosition.x + d->large_platform.size.x;
  if (x < left)
    x = left;
  else if (x > right)
    x = right;
  return (Vector2){x, BossFightPlatformTop(&d->large_platform) -
                          d->character.pickupRadius};
}

// ── 生命周期 ──────────────────────────────────────────────────────────────

// 平台布局：中心大平台 + 多个小平台（左右错落布置于大平台上方，供玩家跳跃
// 躲避弹幕/接字母）
static void BossFightSetupPlatforms(BossFightSceneData *d) {
  const int screenW = d->app->logicWidth;
  // 中心大平台
  InitJumpPlatforms(&d->large_platform, (Vector2){0, 0}, LARGE);
  const float platTop = BOSS_LARGE_TOP;
  d->large_platform.spawnPosition.x =
      (screenW - d->large_platform.size.x) * 0.5f;
  d->large_platform.spawnPosition.y = platTop - d->large_platform.surfaceOffset;

  // 多个小平台：固定错落布局（SMALL 平台 96×48 世界坐标）
  const float sTop[BOSSFIGHT_MAX_SMALL_PLATFORMS] = {300.f, 300.f, 235.f,
                                                     235.f};
  const float sCX[BOSSFIGHT_MAX_SMALL_PLATFORMS] = {100.f, 540.f, 220.f, 420.f};
  d->smallPlatformCount = BOSSFIGHT_MAX_SMALL_PLATFORMS;
  for (int i = 0; i < d->smallPlatformCount; i++) {
    Platform *p = &d->small_platforms[i];
    InitJumpPlatforms(p, (Vector2){0, 0}, SMALL);
    p->spawnPosition.x = sCX[i] - p->size.x * 0.5f;
    p->spawnPosition.y = sTop[i] - p->surfaceOffset;
  }
}

static void BossFightSceneEnter(GameScene *self) {
  BossFightSceneData *d = (BossFightSceneData *)self->data;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 玩家：平台跳跃物理，出生在中心大平台顶面中央
  InitPlayer(d->cat);
  d->cat->app = d->app; // 注入音频宿主（受伤/跳跃音效）
  // 按难度应用最大生命值（Easy/Normal=100，Hard=125）
  PlayerApplyDifficulty(d->cat, d->difficulty);
  // 生命值继承：进入新关卡时恢复上一关剩余 HP（新游戏 playerHealth=0 → 满血）
  if (d->app->playerHealth > 0.0f)
    d->cat->health = d->app->playerHealth;
  else
    d->cat->health = d->cat->maxHealth;
  d->cat->lastHealth = d->cat->health; // 同步受伤检测基准，避免进场误触发
  d->cat->invincibleTimer = 0.0f;      // 进场清除无敌状态

  // 平台布局（中心大平台 + 多个小平台）
  BossFightSetupPlatforms(d);
  const float platTop = BossFightPlatformTop(&d->large_platform);
  d->cat->position =
      (Vector2){d->large_platform.spawnPosition.x +
                    d->large_platform.size.x * 0.5f - d->cat->size.x * 0.5f,
                platTop - d->cat->size.y};
  d->cat->velocity = (Vector2){0, 0};
  d->cat->isOnTheGround = true;

  // 锁定镜头：禁用相机，固定视野（世界坐标 == 逻辑屏幕坐标）
  InitSceneCamera(&d->camera, screenW, screenH, false, CAMERA_FOLLOW_NONE);

  // Character 组件：拼写平台 = 中心大平台中央区域；注入放下落点解析与拼写事件
  d->owner = self->owner;
  CharacterInit(&d->character);
  d->character.app = d->app; // 注入音频宿主（拾取字母音效）
  d->character.dropResolver = BossFightDropResolver;
  d->character.dropCtx = d;
  d->character.onSpellCorrect = BossFightOnSpellCorrect;
  d->character.onSpellWrong = BossFightOnSpellWrong;
  d->character.eventCtx = d;
  d->character.wordPlatform =
      (Rectangle){d->large_platform.spawnPosition.x +
                      d->large_platform.size.x * 0.5f - 90.0f,
                  platTop, 180.0f, 40.0f};

  // 按难度加载词库（简单/普通 CET4，困难 CET6；从内嵌资源）
  const char *relPath =
      (d->difficulty >= 2) ? "assets/words/CET6.txt" : "assets/words/CET4.txt";
  CharacterLoadBankEmbedded(&d->character, relPath);

  // 学习机制：绑定全局错词本/间隔重复抽词（跨关卡共享；
  // 新游戏已在开始菜单重置，见 StudyReset）
  if (d->app->study) {
    StudyRebind(d->app->study, &d->character.bank);
    d->app->study->currentLevel = d->level;
    d->character.study = d->app->study;
  }

  // boss：屏幕上方居中出生，覆盖实体默认 hp=3 为百分比制（docs：正确拼写
  // 固定扣除 boss 20% 生命值）
  InitBoss(d->boss,
           (Vector2){(screenW - d->boss->size.x) * 0.5f, BOSS_PATROL_Y});
  d->boss->hp = BOSS_HP_MAX;
  d->bossDir = 1;
  d->isBossDead = false;
  d->bossCanShoot = false;
  d->afkGrace = 0.0f;
  d->shootTimer = 0.0f;
  d->bulletCount = 0;
  // 保留字段（Timer）：实际计时用 dt 驱动 float 计时器（暂停时 onUpdate
  // 冻结，dt=0 时计时自然暂停，不会因 wall-clock 计时器在暂停期间继续走表）
  InitTimer(&d->boss_afkTimer);
  InitTimer(&d->boss_shootTimer);

  // 难度预计算伤害（仅困难扣血，值见 game_config 的 BOSS_WRONG_PENALTY_HARD；
  // 弹幕为浮动伤害 3~8，在发射时逐颗随机，不随难度缩放）
  d->wrongPenalty = (d->difficulty >= 2) ? BOSS_WRONG_PENALTY_HARD : 0.0f;

  // 多挖空拼写：按难度决定挖空数量（简单 2 / 普通 3 / 困难 4，短词自动收敛）
  d->character.blankCount = 2 + d->difficulty;

  // 初始谜题 + 初始字母批 + 隐藏暴击窗口 + 重置关卡状态
  CharacterSetupPuzzle(&d->character);
  d->timeLeft = BOSS_CRIT_WINDOW;
  d->fallingCount = 0;
  d->transitionRequested = false;
  d->bossSource = AnimationUpdate(&d->boss->animations[BOSS_MOVE], 0.f);
  BossFightSpawnWave(d);

  // 隐式全局计时器：进入第一关开始计时（boss 战最低在第 20 关，防御性保留）
  if (d->level == 1)
    SpeedrunStart((GameApp *)d->app);
}

static void BossFightSceneUpdate(GameScene *self, float dt) {
  BossFightSceneData *d = (BossFightSceneData *)self->data;

  // 玩家 HP 归零 → 失败（只请求一次）
  if (d->cat->health <= 0.0f) {
    if (!d->transitionRequested) {
      d->transitionRequested = true;
      GameStackReplace(self->owner, FailSceneCreate(d->app));
    }
    return;
  }

  // 隐藏暴击窗口：持续递减（关卡内不显示倒计时，docs：不设倒计时提示）
  if (d->timeLeft > 0.0f)
    d->timeLeft -= dt;

  // 无敌时间递减（弹幕命中后给予的免伤窗口）
  if (d->cat->invincibleTimer > 0.0f) {
    d->cat->invincibleTimer -= dt;
    if (d->cat->invincibleTimer < 0.0f)
      d->cat->invincibleTimer = 0.0f;
  }

  // 字母批仅在「正确填一个挖空后」掉落下一批（见 BossFightOnSpellCorrect），
  // 无需按时间周期掉落

  // boss：巡逻 + 弹幕发射
  BossFightUpdateBoss(d, dt);

  // 下落字母：落地并入 character（先于拾取判定，落地当帧即可拾取）
  BossFightUpdateFalling(d, dt);

  // 学习机制：复习横幅计时递减
  CharacterUpdateReview(&d->character, dt);

  // 玩家物理 + 字母拾取/放下/拼写判定
  BossFightUpdatePlayer(d, dt);
  CharacterUpdate(&d->character, d->cat);
  if (d->transitionRequested)
    return; // 已请求场景切换，本帧不再继续

  // 弹幕更新（位移 + 命中玩家扣血）
  BossFightUpdateBullets(d, dt);

  // 玩家动画帧
  d->rec =
      AnimationUpdate(&d->cat->animations[d->cat->playerAnimationState], dt);
}

// 关卡 HUD：左上角关卡号、顶部单词提示（挖空 + 词性 + 中文释义）、boss 生命
// 值条、左下角玩家生命值条、右上角 ESC 提示、底部操作提示。关卡内不显示
// 倒计时（docs：隐藏倒计时）。背景为 RAYWHITE，文字用黑色保证可读。
static void BossFightDrawHud(BossFightSceneData *d) {
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;
  const float margin = 12.0f;

  // 左上角：当前关卡编号
  HudDrawLevel(d->app, d->level);

  // 顶部居中：挖空单词 + 词性
  const int hintSize = 28;
  char hint[128];
  snprintf(hint, sizeof(hint), "%s   (%s)", d->character.revealed,
           d->character.entry.pos);
  GameAppDrawText(d->app, hint,
                  (screenW - GameAppMeasureText(d->app, hint, hintSize)) / 2,
                  (int)margin, hintSize, BLACK);
  // 中文释义（像素字体含中文字形）
  const int meaningSize = 18;
  GameAppDrawText(
      d->app, d->character.entry.meaning,
      (screenW -
       GameAppMeasureText(d->app, d->character.entry.meaning, meaningSize)) /
          2,
      (int)(margin + hintSize + 6), meaningSize, BLACK);

  // boss 生命值条（顶部单词下方）：让玩家直观看到还需拼写几次
  const int barW = 220;
  const int barH = 14;
  const int barX = (screenW - barW) / 2;
  const int barY = (int)(margin + hintSize + 6) + meaningSize + 8;
  float ratio = (float)d->boss->hp / (float)BOSS_HP_MAX;
  if (ratio < 0.0f)
    ratio = 0.0f;
  if (ratio > 1.0f)
    ratio = 1.0f;
  DrawRectangle(barX - 2, barY - 2, barW + 4, barH + 4, DARKGRAY);
  DrawRectangle(barX, barY, (int)(barW * ratio), barH,
                ratio > 0.5f ? GREEN : (ratio > 0.25f ? ORANGE : RED));
  const char *bossLabel = "BOSS";
  GameAppDrawText(d->app, bossLabel,
                  barX - 8 - GameAppMeasureText(d->app, bossLabel, 14), barY,
                  14, BLACK);

  // 左下角：玩家生命值条（全局 HUD，传入继承后的当前 HP）
  HudDrawHealthBar(d->app, d->cat->health, d->cat->maxHealth);

  // 右上角：ESC 暂停提示（全局 HUD）
  HudDrawEscHint(d->app);

  // 底部居中：操作提示（抬升到 HP 条上方，避免与左下角/右下角重合）
  const char *help = "Pick & Drop : Z";
  const int helpSize = 16;
  const int helpY = screenH - (int)margin - 16 - helpSize - 6;
  GameAppDrawText(d->app, help,
                  (screenW - GameAppMeasureText(d->app, help, helpSize)) / 2,
                  helpY, helpSize, BLACK);

  // 拼错复习横幅（学习机制，居中偏上）
  CharacterDrawReviewBanner(&d->character, d->app);
}

static void BossFightSceneDraw(GameScene *self) {
  BossFightSceneData *d = (BossFightSceneData *)self->data;

  // 锁定镜头（禁用相机）：Begin/End 为空操作，直接以世界坐标（== 屏幕坐标）绘制
  BeginSceneCamera(&d->camera);

  // 平台（中心大平台 + 多个小平台）
  DrawPlatform(&d->large_platform);
  for (int i = 0; i < d->smallPlatformCount; i++)
    DrawPlatform(&d->small_platforms[i]);

  // 拼写平台：中央绿色高亮 + 提示
  DrawRectangleRec(d->character.wordPlatform, Fade(GREEN, 0.75f));
  DrawRectangleLinesEx(d->character.wordPlatform, 1.0f, GREEN);
  CharacterDrawSpellHint(&d->character, d->app);

  // 空中下落字母（尚未并入 character 的，含落地等待并入的）
  for (int i = 0; i < d->fallingCount; i++) {
    BossFightFallingLetter *f = &d->falling[i];
    if (f->merged)
      continue;
    DrawCircleV(f->pos, d->character.pickupRadius, Fade(SKYBLUE, 0.85f));
    char txt[2] = {f->ch, '\0'};
    const int fs = 24;
    const int tw = GameAppMeasureText(d->app, txt, fs);
    GameAppDrawText(d->app, txt, (int)(f->pos.x - tw * 0.5f),
                    (int)(f->pos.y - fs * 0.5f), fs, DARKBLUE);
  }

  // 已落地字母（由 Character 组件绘制，统一圆形 + 字符样式）
  CharacterDrawLetters(&d->character, d->app);

  // boss（先绘制 boss 再绘制玩家：确保玩家图层在 boss 之上，boss.h 注意）
  if (!d->isBossDead)
    DrawBoss(d->boss, d->bossSource);

  // 玩家
  DrawPlayer(d->cat, d->rec);

  // 头顶字母 + 虚线引导回拼写平台
  CharacterDrawHeld(&d->character, d->app, d->cat);

  // 弹幕
  for (int i = 0; i < d->bulletCount; i++)
    DrawBullet(&d->bullets[i]);

  EndSceneCamera(&d->camera);

  // 全局 HUD（固定逻辑屏幕坐标）
  BossFightDrawHud(d);
}

static void BossFightSceneExit(GameScene *self) {
  BossFightSceneData *d = (BossFightSceneData *)self->data;
  // 保存当前 HP 供下一关继承（失败/回菜单时由开始场景重置为 0）
  ((GameApp *)d->app)->playerHealth = d->cat->health;
  // 卸载本场景加载的资源（与 onEnter 一一配对）
  UnloadTexture(d->cat->idleTexture);
  UnloadTexture(d->cat->runTexture);
  UnloadTexture(d->cat->jumpTexture);
  UnloadTexture(d->cat->sleepTexture);
  UnloadTexture(d->cat->hitTexture);
  if (d->large_platform.platformTexture.id != 0)
    UnloadTexture(d->large_platform.platformTexture);
  for (int i = 0; i < d->smallPlatformCount; i++)
    if (d->small_platforms[i].platformTexture.id != 0)
      UnloadTexture(d->small_platforms[i].platformTexture);
  if (d->boss && d->boss->bossTexture.id != 0)
    UnloadTexture(d->boss->bossTexture);
  // 弹幕贴图：InitBullet 每颗独立加载，逐一卸载配对
  for (int i = 0; i < BOSSFIGHT_MAX_BULLETS; i++)
    if (d->bullets[i].bulletTexture.id != 0)
      UnloadTexture(d->bullets[i].bulletTexture);
  CharacterFreeBank(&d->character);
  // 注意：boss / bullets / cat 由工厂分配，栈只释放 data，此处手动释放配对
  free(d->boss);
  free(d->bullets);
  free(d->cat);
}

GameScene *BossFightSceneCreate(const GameApp *app, int difficulty, int level) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  if (scene == NULL) {
    return NULL;
  }
  BossFightSceneData *data =
      (BossFightSceneData *)calloc(1, sizeof(BossFightSceneData));
  if (data == NULL) {
    free(scene);
    return NULL;
  }
  data->app = app;
  data->difficulty = difficulty;
  data->level = level;
  // 指针字段：boss / bullets / cat 由本工厂分配，Exit 释放（栈只释放 data）
  data->boss = (Boss *)calloc(1, sizeof(Boss));
  data->bullets = (Bullet *)calloc(1, sizeof(Bullet) * BOSSFIGHT_MAX_BULLETS);
  data->cat = (Player *)calloc(1, sizeof(Player));
  if (data->boss == NULL || data->bullets == NULL || data->cat == NULL) {
    free(data->boss);
    free(data->bullets);
    free(data->cat);
    free(data);
    free(scene);
    return NULL;
  }

  scene->name = "BossFightScene";
  scene->data = data;
  scene->flags = GAME_SCENE_DRAW_WHEN_HIDDEN; // 暂停时可见
  scene->pauseable = true;                    // 可暂停
  scene->onEnter = BossFightSceneEnter;
  scene->onDraw = BossFightSceneDraw;
  scene->onUpdate = BossFightSceneUpdate;
  scene->onExit = BossFightSceneExit;
  return scene;
}
