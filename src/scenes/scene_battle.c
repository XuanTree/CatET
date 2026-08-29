#include "game.h"
#include <math.h>
#include <raylib.h>
#include <stdlib.h>
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// 战斗场景（回合制，docs/game_instructions.md §战斗场景）：
//   以转场（淡入）压入平台跳跃关卡之上，作为一次独立的小型对决：
//   - 玩家回合：屏幕上方给出 3 个候选单词与 1 个「词性+汉语」提示，玩家用
//     左右键（A/D 或 ←/→）选择、Z 确认；选对答对数 +1，选错扣除生命值。
//   - 敌怪回合：敌怪先说话（对话框逐字输出，X 可跳过），停顿 0.30s（期间敌怪
//     原位置快速旋转一周作蓄力提示）后随机选一种弹幕 pattern（共 10 种）发射，
//     玩家可在两个小平台上移动躲避；所有弹幕消失后回到玩家回合。
//   胜利条件 = 答对 totalRounds（1~4）个单词（docs：拼写正确一定数量后进入
//   下一关）：以玩家回合为主，最后一次答对即立即胜利，不再等敌怪释放完最后
//   一波攻击。胜利后画面停顿 0.8s（预留胜利音效）、
//   enemy->isAlive=false（“删除该敌怪”），再经淡出转场 Pop 回平台关卡；
//   玩家 HP 归零（拼写错误或弹幕命中）：失败，Replace 到 FailScene。
//   难度每提高一档：拼写错误惩罚 +50%、弹幕伤害 +100%（docs 玩法说明）。
//   （玩家最大生命值 +25%/档为全局开局加成，不属于战斗场景职责，未在此处理）
// ─────────────────────────────────────────────────────────────────────────────

// C语言真是本质宏孩儿啊
// 不过,后续调整数值的时候,只需要改改宏也是挺方便的

// ── 战斗常量 ──────────────────────────────────────────────────────────────
#define BATTLE_MAX_BULLETS 24 // 弹幕数组上限
#define BATTLE_VOLLEY_BASE 4  // 弹幕数量下限（每难度 +2）
#define BATTLE_VOLLEY_RANDOM_EXTRA                                             \
  2 // 弹幕数量随机增量（在 [lo, lo+extra] 内随机）
#define BATTLE_BULLET_SPEED 274.f      // 弹幕飞行速度（世界坐标/秒）
#define BATTLE_WRONG_PENALTY_BASE 20.f // 基础拼写错误惩罚（×1.5^难度）
#define BATTLE_GRAVITY 980.f  // 与 player.c GRAVITY 一致（回合内仅下落用）
#define BATTLE_GROUND_H 50    // 地面高度（与各关卡一致，顶面 y=480-50）
#define BATTLE_ENEMY_TOP 20.f // 敌怪固定于屏幕最上方
#define BATTLE_SMALL_Y1 215   // 两个小平台固定高度（x 随机）
#define BATTLE_SMALL_Y2 335   // 两个小平台固定高度（x 随机）
#define BATTLE_PLAT_MARGIN 40 // 平台距屏幕边缘的最小间距
// 胜利所需答对单词数随机（1~4，docs：拼写正确一定数量后进入下一关）
#define BATTLE_ROUNDS_MIN 1   // 所需答对单词数下限
#define BATTLE_ROUNDS_MAX 4   // 所需答对单词数上限
#define BATTLE_WIN_DELAY 0.8f // 胜利后画面停顿时长（预留胜利音效播放窗口）
// 敌怪说话机制（dialogue 系统，见 dialogue.h）：
#define BATTLE_DIALOGUE_CHAR_TIME 0.04f // 逐字输出的每字间隔（秒，较快吐字）
#define BATTLE_DIALOGUE_MAX_TIME                                               \
  2.5f // 对话框最长存在时长（秒，含逐字+保持，X 可跳过）
#define BATTLE_ATTACK_DELAY                                                    \
  0.30f // 每波攻击前的停顿时长（秒，期间敌怪旋转蓄力）
// 无敌时间：玩家被弹幕命中后获得的免伤窗口（秒），期间弹幕穿身而过，
#define BATTLE_INVINCIBLE_TIME 1.5f
// 敌怪每回合多波次攻击：
#define BATTLE_WAVES_BASE 2   // 每回合最少攻击波次
#define BATTLE_WAVES_RANDOM 2 // 波次随机增量（[0, BATTLE_WAVES_RANDOM)）

// 战斗回合状态
typedef enum BattlePhase {
  BATTLE_PHASE_PLAYER_TURN = 0, // 玩家回合：三选一选词
  BATTLE_PHASE_ENEMY_TURN,      // 敌怪回合：玩家躲避弹幕
  BATTLE_PHASE_WIN,             // 胜利：标记敌怪删除并 Pop 返回
  BATTLE_PHASE_LOSE,            // 失败：Replace 到失败场景
} BattlePhase;

// 上回合选词结果（敌怪回合期间在 HUD 顶部显示反馈）
typedef enum BattleResult {
  BATTLE_RESULT_NONE = 0,
  BATTLE_RESULT_CORRECT,
  BATTLE_RESULT_WRONG,
} BattleResult;

// 敌怪回合子阶段：说话（逐字）→ 保持显示 → 停顿 0.45s → 弹幕躲避
typedef enum EnemyTalkStage {
  ENEMY_TALK_TYPING = 0, // 敌怪说话：逐个字符输出（X 跳过）
  ENEMY_TALK_HOLD,       // 说话完毕：完整显示 2s（X 跳过）
  ENEMY_ATTACK_WINDUP,   // 停顿 BATTLE_ATTACK_DELAY 后发射弹幕
  ENEMY_DODGE,           // 弹幕躲避：玩家可移动
} EnemyTalkStage;

// 创建战斗场景私有化数据
typedef struct BattleSceneData {
  const GameApp *app;
  Player *player; // 平台关卡的真实玩家指针（战斗直接操作，不拥有）
  Enemy *enemy;   // 平台关卡的真实敌怪指针（冻结/显示/胜利后删除）
  GameScene
      *returnScene; // 战斗结束返回的关卡场景（设计文档约定；实际经 Pop 返回）

  SceneCamera camera; // 锁定镜头（禁用，固定视野，战斗在逻辑屏幕坐标进行）
  int difficulty;

  Rectangle playerSource; // 玩家当前动画帧源矩形
  Rectangle enemySource;  // 敌怪当前动画帧源矩形

  // 躲避弹幕的两个小平台（数目固定，x 随机）
  Platform platform_s1;
  Platform platform_s2;

  // 弹幕（敌怪回合发射，固定数组复用槽位）
  Bullet bullets[BATTLE_MAX_BULLETS];
  int bulletCount; // 当前已占用弹幕槽位数（含失效待复用）

  // 敌怪说话 + 弹幕发射流程（敌怪回合）
  EnemyTalkStage enemyStage; // 敌怪回合子阶段
  char dialogue[160];        // 敌怪本次说话的文本（dialogue 系统抽取）
  int charShown;             // 已显示的字符数（逐字输出）
  float typeTimer;           // 逐字输出计时
  float dialogueTimer; // 对话框累计存在时长（上限 BATTLE_DIALOGUE_MAX_TIME）
  float stageTimer;    // 子阶段计时（0.45s 攻击前停顿）
  int attackWave;      // 当前攻击波次（0 起始，每波发射一轮弹幕）
  int attackWaveTotal; // 本回合总攻击波次（随难度随机）

  // 词库与三选一单词（词库经 WordsBank 加载/释放）
  WordsBank bank;
  WordEntry options[3]; // 三个候选单词（含正确与干扰）
  int answerIndex;      // 正确单词在 options 中的下标
  int selectIndex;      // 玩家当前选中的选项下标

  // 按难度预计算的伤害/惩罚（避免逐帧 powf）；弹幕为浮动伤害 3~9，
  // 在 FireVolley 发射后逐颗随机，不再按难度预计算
  float wrongPenalty;

  // 战斗状态
  BattlePhase phase;
  BattleResult lastResult; // 上回合选词结果（敌怪回合期间显示反馈）
  int totalRounds;         // 胜利所需答对单词数（随机 1~4，见 BattleEnter）
  int correctCount;        // 已答对单词数（达到 totalRounds 即胜利）
  float winTimer;          // 胜利停顿倒计时（BATTLE_WIN_DELAY，音效无效时兜底）
  bool winSoundPlayed;     // 胜利音效是否已播放（防止重复触发）
  bool transitionRequested; // 已请求 Pop/Replace，防止同帧重复切换

  // 返回平台关卡时恢复的玩家状态（避免战斗结束“瞬移”）
  Vector2 returnPos;
  Vector2 returnVel;
  bool returnOnGround;
} BattleSceneData;

// ── 三选一单词 ─────────────────────────────────────────────────────────────

// 从词库抽取答案词与两个不同的干扰词，洗牌放入 options[3] 并记录答案下标。
// 词库为空/单词不足时用兜底词条（防御，保证场景仍可运行，与 Character 的
// "cat" 兜底思路一致）。
static void SetupChoices(BattleSceneData *d) {
  static const WordEntry kFallback[3] = {
      {"cat", "n. 猫", "n."},
      {"dog", "n. 狗", "n."},
      {"run", "v. 跑", "v."},
  };

  // 答案词：从词库随机抽取，词库为空时用兜底
  const WordEntry *ans = WordsBankPickRandom(&d->bank);
  if (!ans)
    ans = &kFallback[0];

  // 两个与答案不同的干扰词（词库不足时用兜底补齐，保证必有 3 个选项）
  const WordEntry *dist[2] = {&kFallback[1], &kFallback[2]};
  int got = 0;
  int guard = 0;
  while (got < 2 && guard < 300) {
    guard++;
    const WordEntry *w = WordsBankPickRandom(&d->bank);
    if (!w)
      break;
    if (strcmp(w->word, ans->word) == 0)
      continue;
    bool dup = false;
    for (int k = 0; k < got; k++) {
      if (strcmp(w->word, dist[k]->word) == 0) {
        dup = true;
        break;
      }
    }
    if (dup)
      continue;
    dist[got++] = w;
  }

  // 洗牌：答案随机放入三个槽位之一，其余两槽放干扰词（不足时退化为答案，防御）
  int answerSlot = genRandomNum(3);
  d->answerIndex = answerSlot;
  bool used[3] = {false, false, false};
  used[answerSlot] = true;
  d->options[answerSlot] = *ans;
  int di = 0;
  for (int i = 0; i < 3; i++) {
    if (used[i])
      continue;
    const WordEntry *pick = (di < got) ? dist[di++] : ans;
    d->options[i] = *pick;
  }
  d->selectIndex = 0;
}

// ── 弹幕 ─────────────────────────────────────────────────────────────────

// 敌怪发射弹幕：每次随机选择一种 pattern，数量在难度区间内随机。
static void FireVolley(BattleSceneData *d) {
  Player *player = d->player;
  Enemy *enemy = d->enemy;
  int lo =
      BATTLE_VOLLEY_BASE + 2 * d->difficulty; // 下限 easy 4 / normal 6 / hard 8
  int hi = lo + BATTLE_VOLLEY_RANDOM_EXTRA;   // 上限 +2
  int count = lo + genRandomNum(hi - lo + 1); // 数量随机
  Vector2 origin = {enemy->position.x + enemy->size.x * 0.5f,
                    enemy->position.y + enemy->size.y * 0.5f};
  Vector2 target = {player->position.x + player->size.x * 0.5f,
                    player->position.y + player->size.y * 0.5f};
  // 随机 pattern 写入弹幕数组，返回实际生成数（伤害先传 0，随后逐颗随机）
  d->bulletCount =
      BulletPatternFire(d->bullets, BATTLE_MAX_BULLETS, BulletPatternRoll(),
                        origin, target, count, BATTLE_BULLET_SPEED, 0.f);
  // 浮动伤害：每颗弹幕随机 3~9 点伤害（docs 难度弹幕 +100% 改为固定浮动区间）
  for (int i = 0; i < d->bulletCount; i++)
    d->bullets[i].damage = BulletRollDamage();
}

// 进入敌怪回合：敌怪先说话（逐字输出，X 可跳过），完整显示 2s 后停顿 0.45s，
// 再随机选一种弹幕 pattern 发射。
static void StartEnemyTurn(BattleSceneData *d) {
  d->phase = BATTLE_PHASE_ENEMY_TURN;
  d->enemyStage = ENEMY_TALK_TYPING;
  // 从 dialogue 系统随机抽取一句敌怪台词
  const char *line = getDialogue();
  snprintf(d->dialogue, sizeof(d->dialogue), "%s", line ? line : "");
  d->charShown = 0;
  d->typeTimer = 0.f;
  d->dialogueTimer = 0.f;
  d->stageTimer = 0.f;
  d->bulletCount = 0;
  // 本回合多波次攻击：基础 1 + 难度 + 随机增量（easy 1-2 / normal 2-3 / hard
  // 3-4）
  d->attackWave = 0;
  d->attackWaveTotal =
      BATTLE_WAVES_BASE + d->difficulty + genRandomNum(BATTLE_WAVES_RANDOM);
}

// 进入玩家回合：生成新的三选一单词，玩家不可移动（仅左右选择 + Z 确认）。
static void StartPlayerTurn(BattleSceneData *d) {
  d->phase = BATTLE_PHASE_PLAYER_TURN;
  SetupChoices(d);
}

// 对玩家造成一次伤害：扣血、钳制下限、同步受伤检测基准并播放受伤音效。
// 同步 lastHealth 是关键——选词/弹幕直接扣血后若不同步，下一帧
// UpdatePlayer 的「生命值下降检测」会再次触发击退与受伤音效（表现为
// 连续两次受伤，甚至把玩家击退进弹幕流造成第二次伤害）。
static void BattleDamagePlayer(BattleSceneData *d, float amount) {
  Player *player = d->player;
  player->health -= amount;
  if (player->health < 0.f)
    player->health = 0.f;
  player->lastHealth = player->health; // 同步基准，避免 UpdatePlayer 二次触发
  PlayerTriggerHit(player); // 直接触发 HIT 动画（同步基准后 UpdatePlayer 不会
                            // 走伤害检测，必须手动触发，否则受伤无 hit 动画）
  GameAppPlaySound(d->app, d->app->catHitSound, d->app->catHitSoundValid);
}

// ── 回合更新 ──────────────────────────────────────────────────────────────

// 玩家不可移动阶段（选词/说话/攻击停顿）：仅受重力下落、推进受伤动画并
// 更新动画状态，避免回合切换时玩家悬空。
static void UpdatePlayerFrozen(BattleSceneData *d, float dt) {
  Player *player = d->player;
  player->velocity.x = 0.f;
  player->velocity.y += BATTLE_GRAVITY * dt;
  player->position.y += player->velocity.y * dt;
  player->isOnTheGround = false;
  PlayerCollision(player, &d->platform_s1);
  PlayerCollision(player, &d->platform_s2);
  // 地面宽 = logicWidth：与 BattleDraw 中 DrawRectangle(0, ..., screenW, ...)
  // 一致
  GroundCollision(player, (float)d->app->logicWidth);
  // 防御：掉到地面以下时送回地面顶面
  const float groundTop = (float)(d->app->logicHeight - BATTLE_GROUND_H);
  if (player->position.y > groundTop + 100.f) {
    player->position.y = groundTop - player->size.y;
    player->velocity.y = 0.f;
    player->isOnTheGround = true;
  }
  // 受伤动画计时（战斗开始时的残留 hitTimer 继续走完）
  if (player->hitTimer > 0.f) {
    player->hitTimer -= dt;
    if (player->hitTimer < 0.f)
      player->hitTimer = 0.f;
  }
  // 受伤优先播放 HIT（不循环），否则空中 JUMP、落地 IDLE
  if (player->hitTimer > 0.f)
    player->playerAnimationState = HIT;
  else if (!player->isOnTheGround)
    player->playerAnimationState = JUMP;
  else
    player->playerAnimationState = IDLE;
}

// 玩家回合：无水平输入（仅受重力下落，避免回合切换时悬空），
// 左右键移动选择、Z 确认选词。
static void UpdatePlayerTurn(BattleSceneData *d, float dt) {
  Player *player = d->player;
  Enemy *enemy = d->enemy;

  // 仅下落：玩家回合不可移动（docs：进入单词选择环节时玩家无法移动）
  UpdatePlayerFrozen(d, dt);

  // 左右键选择单词（循环切换）；切换选项时播放 UI 音效
  const int prevSelect = d->selectIndex;
  if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))
    d->selectIndex = (d->selectIndex + 2) % 3; // 左移
  else if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT))
    d->selectIndex = (d->selectIndex + 1) % 3; // 右移
  if (d->selectIndex != prevSelect) {
    GameAppPlaySound(d->app, d->app->uiSound, d->app->uiSoundValid);
  }

  // Z 确认选词（选对推进答对进度，选错扣血不推进）
  if (IsKeyPressed(KEY_Z)) {
    // 选择答案播放 UI 音效（ui_sound.ogg）
    GameAppPlaySound(d->app, d->app->uiSound, d->app->uiSoundValid);
    if (d->selectIndex == d->answerIndex) {
      // 选对：答对数 +1（胜利条件为答对 totalRounds 个单词）
      d->lastResult = BATTLE_RESULT_CORRECT;
      d->correctCount++;
    } else {
      // 选错：不推进答对进度，扣除生命值（随难度递增）。统一走
      // BattleDamagePlayer 扣血并同步 lastHealth——避免进入敌怪回合后
      // UpdatePlayer 检测到血量下降触发多余的击退/受伤效果，把玩家弹进弹幕
      // 造成第二次伤害。
      BattleDamagePlayer(d, d->wrongPenalty);
      if (player->health <= 0.f) {
        d->phase = BATTLE_PHASE_LOSE;
        return;
      }
      d->lastResult = BATTLE_RESULT_WRONG;
    }

    // 以玩家回合为主：答对次数达到 totalRounds 即立即胜利，不再进入敌怪回合
    // 的最后一波攻击（docs：敌怪回合）
    if (d->correctCount >= d->totalRounds) {
      d->phase = BATTLE_PHASE_WIN;
      d->enemy->isAlive = false; // “删除该敌怪”（胜利停顿期间不再绘制）
      return;
    }

    // 未达到所需答对数：不论对错，敌怪都有回合（docs：敌怪回合）
    StartEnemyTurn(d);
  }

  d->playerSource =
      AnimationUpdate(&player->animations[player->playerAnimationState], dt);
  d->enemySource = AnimationUpdate(&enemy->animations[ENEMY_MOVE], dt);
}

// 敌怪说话：逐字输出台词（较快吐字），X 跳过；对话框累计存在时长达到
// BATTLE_DIALOGUE_MAX_TIME（含逐字 + 完整保持）后自动转攻击。
static void UpdateEnemyDialogue(BattleSceneData *d, float dt) {
  const int len = (int)strlen(d->dialogue);
  d->dialogueTimer += dt;

  // 对话框最长存在时长：到时直接转攻击（即使仍在逐字输出）
  if (d->dialogueTimer >= BATTLE_DIALOGUE_MAX_TIME) {
    d->enemyStage = ENEMY_ATTACK_WINDUP;
    d->stageTimer = BATTLE_ATTACK_DELAY;
    return;
  }

  if (d->enemyStage == ENEMY_TALK_TYPING) {
    // X 跳过逐字输出，直接进入攻击停顿
    if (IsKeyPressed(KEY_X)) {
      d->charShown = len;
      d->enemyStage = ENEMY_ATTACK_WINDUP;
      d->stageTimer = BATTLE_ATTACK_DELAY;
      return;
    }
    // 逐个字符输出（较快吐字）
    d->typeTimer += dt;
    while (d->typeTimer >= BATTLE_DIALOGUE_CHAR_TIME && d->charShown < len) {
      d->typeTimer -= BATTLE_DIALOGUE_CHAR_TIME;
      d->charShown++;
    }
    if (d->charShown >= len)
      d->enemyStage = ENEMY_TALK_HOLD; // 完整显示，等待 dialogueTimer 到上限
    return;
  }

  // ENEMY_TALK_HOLD：完整显示中，X 跳过；到时（dialogueTimer 上限）自动转攻击
  if (IsKeyPressed(KEY_X)) {
    d->enemyStage = ENEMY_ATTACK_WINDUP;
    d->stageTimer = BATTLE_ATTACK_DELAY;
    return;
  }
}

// 玩家可移动更新：敌怪攻击回合全程通用的玩家物理（说话/停顿/弹幕躲避子阶段
// 均可用）。含水平移动、平台碰撞、屏幕边界钳制与掉出底部送回地面。
static void UpdatePlayerMovable(BattleSceneData *d, float dt) {
  Player *player = d->player;
  UpdatePlayer(player, dt);
  player->isOnTheGround = false;
  PlayerCollision(player, &d->platform_s1);
  PlayerCollision(player, &d->platform_s2);
  // 地面宽 = logicWidth：与 BattleDraw 中 DrawRectangle(0, ..., screenW, ...)
  // 一致
  GroundCollision(player, (float)d->app->logicWidth);

  // 水平边界钳制：玩家不能走出屏幕
  if (player->position.x < 0.f)
    player->position.x = 0.f;
  if (player->position.x + player->size.x > (float)d->app->logicWidth)
    player->position.x = (float)d->app->logicWidth - player->size.x;

  // 防御：掉出底部送回地面
  const float groundTop = (float)(d->app->logicHeight - BATTLE_GROUND_H);
  if (player->position.y > groundTop + 100.f) {
    player->position.y = groundTop - player->size.y;
    player->velocity = (Vector2){0.f, 0.f};
    player->isOnTheGround = true;
  }
}

// 弹幕躲避阶段：玩家可移动，更新弹幕并检测命中；所有弹幕消失后本回合结束。
static void UpdateEnemyDodge(BattleSceneData *d, float dt) {
  Player *player = d->player;

  // 玩家移动（躲避弹幕）
  UpdatePlayerMovable(d, dt);

  // 更新弹幕：位移 + 飞出屏幕失效 + 命中玩家扣血并销毁
  bool anyActive = false;
  for (int i = 0; i < d->bulletCount; i++) {
    Bullet *b = &d->bullets[i];
    if (!b->isActive)
      continue;
    UpdateBullet(b, dt);
    // 飞出屏幕外 → 失效（docs：弹幕消失包括飞出屏幕外）
    if (b->position.x + b->size.x < 0.f ||
        b->position.x > (float)d->app->logicWidth ||
        b->position.y + b->size.y < 0.f ||
        b->position.y > (float)d->app->logicHeight) {
      b->isActive = false;
      continue;
    }
    // 命中玩家 → 扣血并销毁（docs：被玩家击中消失）
    Rectangle pr = {player->position.x, player->position.y, player->size.x,
                    player->size.y};
    Rectangle br = {b->position.x, b->position.y, b->size.x, b->size.y};
    if (CheckCollisionRecs(pr, br)) {
      // 无敌期间：弹幕穿身而过，不扣血、不销毁（命中后获得短暂免伤窗口，
      // 避免弹幕雨/环形弹幕在同帧或连续数帧内多次命中造成连续扣血）
      if (player->invincibleTimer > 0.f)
        continue;
      BattleDamagePlayer(d, b->damage);
      // 命中后给予无敌时间（绘制时用闪烁表现，见 BattleDraw）
      player->invincibleTimer = BATTLE_INVINCIBLE_TIME;
      b->isActive = false;
      continue;
    }
    anyActive = true;
  }

  // 玩家 HP 归零 → 失败
  if (player->health <= 0.f) {
    d->phase = BATTLE_PHASE_LOSE;
    return;
  }

  // 所有弹幕消失：若还有下一波攻击则进入停顿，否则本回合结束
  if (!anyActive) {
    if (d->attackWave < d->attackWaveTotal) {
      // 下一波攻击：停顿 0.45s 后发射新一轮弹幕
      d->enemyStage = ENEMY_ATTACK_WINDUP;
      d->stageTimer = BATTLE_ATTACK_DELAY;
    } else {
      // 本回合所有波次结束：敌怪攻击回合结束，回到玩家回合继续答题。
      // 胜利判定在玩家回合（答对数达到 totalRounds 即胜利），此处不再判胜。
      StartPlayerTurn(d);
    }
  }
}

// 敌怪回合：说话（逐字 + 保持 + X 跳过）→ 停顿 0.45s → 随机 pattern 发射
// 弹幕 → 玩家躲避。整段都是敌怪的攻击回合，玩家始终允许移动走位（可在说话
// 时预判走位、停顿蓄力时提前站位）；所有弹幕消失后本回合结束。
static void UpdateEnemyTurn(BattleSceneData *d, float dt) {
  Player *player = d->player;
  Enemy *enemy = d->enemy;

  switch (d->enemyStage) {
  case ENEMY_TALK_TYPING:
  case ENEMY_TALK_HOLD:
    // 说话阶段：玩家全程可移动，逐字显示台词；X 可跳过
    UpdatePlayerMovable(d, dt);
    UpdateEnemyDialogue(d, dt);
    break;
  case ENEMY_ATTACK_WINDUP:
    // 停顿 0.45s：玩家仍可移动走位（为躲避本波弹幕预判站位），随后发射弹幕
    UpdatePlayerMovable(d, dt);
    d->stageTimer -= dt;
    if (d->stageTimer <= 0.f) {
      FireVolley(d); // 随机 pattern + 随机数量
      d->attackWave++;
      d->enemyStage = ENEMY_DODGE;
    }
    break;
  case ENEMY_DODGE:
    UpdateEnemyDodge(d, dt);
    break;
  default:
    break;
  }

  d->playerSource =
      AnimationUpdate(&player->animations[player->playerAnimationState], dt);
  d->enemySource = AnimationUpdate(&enemy->animations[ENEMY_MOVE], dt);
}

// ── 生命周期 ──────────────────────────────────────────────────────────────

static void BattleEnter(GameScene *self) {
  BattleSceneData *d = (BattleSceneData *)self->data;
  Player *player = d->player;
  Enemy *enemy = d->enemy;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 保存玩家在平台关卡中的位置/速度，战斗结束返回时恢复（避免“瞬移”）
  d->returnPos = player->position;
  d->returnVel = player->velocity;
  d->returnOnGround = player->isOnTheGround;

  // 冻结敌怪：不受重力、固定在屏幕正上方居中、无法移动但正常播放动画
  enemy->isMovable = false;
  enemy->isAlive = true;
  enemy->position =
      (Vector2){(screenW - enemy->size.x) * 0.5f, BATTLE_ENEMY_TOP};
  enemy->velocity = (Vector2){0.f, 0.f};
  enemy->isOnTheGround = false;

  // 玩家：显示在最下方（地面之上），播放 IDLE
  const float groundTop = (float)(screenH - BATTLE_GROUND_H);
  player->position =
      (Vector2){(screenW - player->size.x) * 0.5f, groundTop - player->size.y};
  player->velocity = (Vector2){0.f, 0.f};
  player->isOnTheGround = true;
  player->playerAnimationState = IDLE;
  // 同步受伤检测基准，避免进场误触发受伤动画
  player->lastHealth = player->health;
  player->invincibleTimer = 0.f; // 进场清除无敌状态（不应从平台关卡带入）

  // 锁定镜头：禁用相机，战斗在固定逻辑屏幕坐标进行
  InitSceneCamera(&d->camera, screenW, screenH, false, CAMERA_FOLLOW_NONE);

  // 躲避弹幕的两个小平台（数目固定，x 随机）
  d->platform_s1 = (Platform){0};
  InitJumpPlatforms(&d->platform_s1, (Vector2){0.f, BATTLE_SMALL_Y1}, SMALL);
  int range1 = screenW - 2 * BATTLE_PLAT_MARGIN - (int)d->platform_s1.size.x;
  if (range1 < 1)
    range1 = 1;
  d->platform_s1.spawnPosition.x =
      (float)(BATTLE_PLAT_MARGIN + genRandomNum(range1));

  d->platform_s2 = (Platform){0};
  InitJumpPlatforms(&d->platform_s2, (Vector2){0.f, BATTLE_SMALL_Y2}, SMALL);
  int range2 = screenW - 2 * BATTLE_PLAT_MARGIN - (int)d->platform_s2.size.x;
  if (range2 < 1)
    range2 = 1;
  d->platform_s2.spawnPosition.x =
      (float)(BATTLE_PLAT_MARGIN + genRandomNum(range2));

  // 按难度加载词库（与其它关卡一致：简单/普通 CET4，困难 CET6；从内嵌资源）
  const char *relPath = "assets/words/CET4.txt";
  if (d->difficulty >= 2)
    relPath = "assets/words/CET6.txt";
  WordsBankLoadEmbedded(&d->bank, relPath);

  // 难度影响：拼写错误惩罚 ×1.5^d（docs 玩法说明）；弹幕为浮动伤害 3~9，
  // 在 FireVolley 发射时逐颗随机，不随难度缩放
  d->wrongPenalty =
      BATTLE_WRONG_PENALTY_BASE * powf(1.5f, (float)d->difficulty);

  // 初始化战斗状态：从玩家回合开始
  d->phase = BATTLE_PHASE_PLAYER_TURN;
  d->lastResult = BATTLE_RESULT_NONE;
  // 本场战斗胜利所需答对单词数（小怪强弱不一：1~4 个）
  d->totalRounds = BATTLE_ROUNDS_MIN +
                   genRandomNum(BATTLE_ROUNDS_MAX - BATTLE_ROUNDS_MIN + 1);
  d->correctCount = 0;
  d->winTimer = BATTLE_WIN_DELAY;
  d->winSoundPlayed = false;
  d->transitionRequested = false;
  d->bulletCount = 0;
  d->attackWave = 0;
  d->attackWaveTotal = BATTLE_WAVES_BASE; // 实际波次由 StartEnemyTurn 随机确定
  d->enemyStage = ENEMY_TALK_TYPING;
  d->charShown = 0;
  d->typeTimer = 0.f;
  d->dialogueTimer = 0.f;
  d->stageTimer = 0.f;
  d->dialogue[0] = '\0';
  d->playerSource =
      AnimationUpdate(&player->animations[player->playerAnimationState], 0.f);
  d->enemySource = AnimationUpdate(&enemy->animations[ENEMY_MOVE], 0.f);
  SetupChoices(d);
}

static void BattleUpdate(GameScene *self, float dt) {
  BattleSceneData *d = (BattleSceneData *)self->data;

  // 无敌时间递减（弹幕命中后给予的免伤窗口，归零后恢复正常受击）
  if (d->player->invincibleTimer > 0.f) {
    d->player->invincibleTimer -= dt;
    if (d->player->invincibleTimer < 0.f)
      d->player->invincibleTimer = 0.f;
  }

  switch (d->phase) {
  case BATTLE_PHASE_PLAYER_TURN:
    UpdatePlayerTurn(d, dt);
    break;
  case BATTLE_PHASE_ENEMY_TURN:
    UpdateEnemyTurn(d, dt);
    break;
  case BATTLE_PHASE_WIN:
    // 胜利：进入时播放 battle_win 音效，音效播放完毕后 Replace 到
    // 「淡出后 Pop」转场，露出下层平台关卡（只请求一次，避免同帧重复切换）。
    // 音效加载失败时用 BATTLE_WIN_DELAY 兜底，防止卡死。
    if (!d->winSoundPlayed) {
      GameAppPlaySound(d->app, d->app->battleWinSound,
                       d->app->battleWinSoundValid);
      d->winSoundPlayed = true;
    }
    if (d->app->battleWinSoundValid) {
      // 音效有效：等待播放完毕。首帧 IsSoundPlaying 可能尚未置位，用
      // winTimer 已开始递减（经过至少 1 帧）作为最小等待，避免立即误判跳转。
      d->winTimer -= dt;
      if (!IsSoundPlaying(d->app->battleWinSound) &&
          d->winTimer < BATTLE_WIN_DELAY && !d->transitionRequested) {
        d->transitionRequested = true;
        GameStackReplace(self->owner, TransitionSceneCreatePop(d->app));
      }
    } else {
      // 音效无效：沿用固定停顿兜底（无音效可播，按原窗口时长停留）
      d->winTimer -= dt;
      if (d->winTimer <= 0.f && !d->transitionRequested) {
        d->transitionRequested = true;
        GameStackReplace(self->owner, TransitionSceneCreatePop(d->app));
      }
    }
    break;
  case BATTLE_PHASE_LOSE:
    // 失败：替换为失败场景（只请求一次）
    if (!d->transitionRequested) {
      d->transitionRequested = true;
      GameStackReplace(self->owner, FailSceneCreate(d->app));
    }
    break;
  default:
    break;
  }
}

// ── HUD / 绘制 ────────────────────────────────────────────────────────────

// 战斗 HUD：左下角生命值条、左上角答对进度、玩家回合的三选一单词与提示。
// 布局说明：敌怪固定居中于屏幕最上方（两侧留白可用），答对进度放左上角；
// 三选一单词框放中上部、操作提示放底部，避免与敌怪/平台重叠。
static void DrawBattleHud(BattleSceneData *d) {
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;
  const float margin = 12.f;
  const int fontSize = 16;

  // 左下角生命值条
  HudDrawHealthBar(d->app, d->player->health, d->player->maxHealth);

  // 左上角：答对进度（敌怪居中于顶部，两侧留白可用）
  if (d->phase == BATTLE_PHASE_PLAYER_TURN) {
    // 答对 totalRounds 个单词即胜利（未达则敌怪回合后继续答题）
    char info[64];
    snprintf(info, sizeof(info), "Correct %d / %d", d->correctCount,
             d->totalRounds);
    GameAppDrawText(d->app, info, (int)margin, (int)margin, fontSize, WHITE);
  } else if (d->phase == BATTLE_PHASE_ENEMY_TURN) {
    // 上回合选词结果反馈（选对绿色 / 选错红色）
    if (d->lastResult == BATTLE_RESULT_CORRECT)
      GameAppDrawText(d->app, "CORRECT!", (int)margin, (int)margin, fontSize,
                      GREEN);
    else if (d->lastResult == BATTLE_RESULT_WRONG)
      GameAppDrawText(d->app, "WRONG!", (int)margin, (int)margin, fontSize,
                      RED);
  }

  if (d->phase == BATTLE_PHASE_PLAYER_TURN) {
    // 三个候选单词框（居中，位于敌怪下方）
    const int boxW = 140;
    const int boxH = 36;
    const int gap = 24;
    const int totalW = boxW * 3 + gap * 2;
    const int startX = (screenW - totalW) / 2;
    const int boxY = 110;

    for (int i = 0; i < 3; i++) {
      Rectangle rec = {startX + i * (boxW + gap), boxY, boxW, boxH};
      const bool selected = (i == d->selectIndex);
      // 选中项高亮；不直接暴露答案（玩家凭「词性+汉语」提示自行判断）
      DrawRectangleRec(rec, selected ? Fade(WHITE, 0.18f) : Fade(WHITE, 0.06f));
      DrawRectangleLinesEx(rec, selected ? 2.f : 1.f, selected ? YELLOW : GRAY);
      const int tw = GameAppMeasureText(d->app, d->options[i].word, 20);
      GameAppDrawText(d->app, d->options[i].word,
                      (int)(rec.x + rec.width * 0.5f) - tw / 2,
                      (int)(rec.y + rec.height * 0.5f) - 10, 20,
                      selected ? WHITE : LIGHTGRAY);
    }

    // 提示：答案的「词性+汉语」（位于三个单词下方中间）
    const WordEntry *ans = &d->options[d->answerIndex];
    char hint[300];
    snprintf(hint, sizeof(hint), "%s  %s", ans->pos, ans->meaning);
    const int hintSize = 18;
    GameAppDrawText(d->app, hint,
                    (screenW - GameAppMeasureText(d->app, hint, hintSize)) / 2,
                    boxY + boxH + 14, hintSize, SKYBLUE);

    // 右下角操作提示（右对齐，避开左下角 HP 条与底部玩家）
    const char *control = "A/D or Arrows : select    Z : confirm";
    GameAppDrawText(d->app, control,
                    screenW - (int)margin -
                        GameAppMeasureText(d->app, control, fontSize),
                    screenH - 24, fontSize, LIGHTGRAY);
  } else if (d->phase == BATTLE_PHASE_ENEMY_TURN &&
             d->enemyStage == ENEMY_DODGE) {
    // 弹幕躲避提示（居中于敌怪下方，仅在躲避阶段显示）
    const char *dodge = "Dodge the bullets!";
    GameAppDrawText(d->app, dodge,
                    (screenW - GameAppMeasureText(d->app, dodge, fontSize)) / 2,
                    90, fontSize, LIGHTGRAY);
  } else if (d->phase == BATTLE_PHASE_WIN) {
    // 胜利停顿期间：居中显示 VICTORY
    const int winSize = 40;
    const char *win = "VICTORY!";
    GameAppDrawText(d->app, win,
                    (screenW - GameAppMeasureText(d->app, win, winSize)) / 2,
                    screenH / 2 - winSize / 2, winSize, GREEN);
  }
}

static void DrawDialogueBox(BattleSceneData *d) {
  if (d->phase != BATTLE_PHASE_ENEMY_TURN)
    return;
  if (d->enemyStage != ENEMY_TALK_TYPING && d->enemyStage != ENEMY_TALK_HOLD)
    return;

  const int screenW = d->app->logicWidth;
  const int boxX = 80; // 居中于敌怪正下方
  const int boxW = screenW - 2 * boxX;
  const int boxH = 76;
  const int boxY = 72; // 敌怪占 20~68，对话框紧随其正下方

  // 白底黑字：白底 + raygui 边框 + 黑色对话文本
  Rectangle box = {(float)boxX, (float)boxY, (float)boxW, (float)boxH};
  DrawRectangleRec(box, WHITE);
  GuiGroupBox(box, NULL);

  // 逐字显示的对话文本（黑色，像素字体）
  char shown[160];
  int n = d->charShown;
  if (n > (int)sizeof(shown) - 1)
    n = (int)sizeof(shown) - 1;
  memcpy(shown, d->dialogue, (size_t)n);
  shown[n] = '\0';
  GameAppDrawText(d->app, shown, boxX + 14, boxY + 12, 18, BLACK);

  // 底部右侧提示 "press x to skip"
  const char *skip = "press x to skip";
  GameAppDrawText(d->app, skip,
                  boxX + boxW - 14 - GameAppMeasureText(d->app, skip, 16),
                  boxY + boxH - 26, 16, DARKGRAY);
}

static void BattleDraw(GameScene *self) {
  BattleSceneData *d = (BattleSceneData *)self->data;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 纯黑色背景 + 略透明淡灰色网格（docs 战斗场景设计）
  DrawRectangle(0, 0, screenW, screenH, BLACK);
  const int grid = 40;
  for (int x = 0; x <= screenW; x += grid)
    DrawLine(x, 0, x, screenH, Fade(LIGHTGRAY, 0.12f));
  for (int y = 0; y <= screenH; y += grid)
    DrawLine(0, y, screenW, y, Fade(LIGHTGRAY, 0.12f));

  // 躲避弹幕的平台与地面
  DrawPlatform(&d->platform_s1);
  DrawPlatform(&d->platform_s2);
  DrawRectangle(0, screenH - BATTLE_GROUND_H, screenW, BATTLE_GROUND_H,
                Fade(LIGHTGRAY, 0.30f));

  // 敌怪：屏幕最上方，无法移动，正常播放动画
  if (d->enemy->isAlive) {
    // 蓄力提示：发射弹幕前的短暂停顿（ENEMY_ATTACK_WINDUP）里，敌怪原位置
    // 快速旋转贴图一圈（0°→360°），随 stageTimer 倒计时推进，提示即将发射。
    float rot = 0.f;
    if (d->phase == BATTLE_PHASE_ENEMY_TURN &&
        d->enemyStage == ENEMY_ATTACK_WINDUP) {
      float t = d->stageTimer / BATTLE_ATTACK_DELAY; // 1→0
      if (t < 0.f)
        t = 0.f;
      rot = (1.f - t) * 360.f;
    }
    DrawEnemy(d->enemy, d->enemySource, rot);
  }
  // 玩家：最下方
  DrawPlayer(d->player, d->playerSource);

  // 弹幕
  for (int i = 0; i < d->bulletCount; i++)
    DrawBullet(&d->bullets[i]);

  // 敌怪对话框（说话/攻击停顿阶段，白底黑字）
  DrawDialogueBox(d);

  // HUD（固定逻辑屏幕坐标）
  DrawBattleHud(d);
}

static void BattleExit(GameScene *self) {
  BattleSceneData *d = (BattleSceneData *)self->data;

  // 释放弹幕纹理（InitBullet 每颗独立加载，逐一卸载配对）
  for (int i = 0; i < BATTLE_MAX_BULLETS; i++)
    if (d->bullets[i].bulletTexture.id != 0)
      UnloadTexture(d->bullets[i].bulletTexture);

  // 释放词库
  WordsBankFree(&d->bank);

  // 释放平台纹理（与 onEnter 加载配对）
  if (d->platform_s1.platformTexture.id != 0)
    UnloadTexture(d->platform_s1.platformTexture);
  if (d->platform_s2.platformTexture.id != 0)
    UnloadTexture(d->platform_s2.platformTexture);

  // 恢复玩家在平台关卡中的位置/速度（避免战斗结束“瞬移”）
  if (d->player) {
    d->player->position = d->returnPos;
    d->player->velocity = d->returnVel;
    d->player->isOnTheGround = d->returnOnGround;
  }
  // 恢复敌怪可移动（若战斗失败被 Replace 时敌怪仍存活，防御性恢复）
  if (d->enemy)
    d->enemy->isMovable = true;
}

GameScene *BattleSceneCreate(const GameApp *app, Player *player, Enemy *enemy,
                             GameScene *returnScene, int difficulty) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  if (scene == NULL) {
    return NULL;
  }
  BattleSceneData *data = (BattleSceneData *)calloc(1, sizeof(BattleSceneData));
  if (data == NULL) {
    free(scene);
    return NULL;
  }
  data->app = app;
  data->player = player;
  data->enemy = enemy;
  data->returnScene = returnScene;
  data->difficulty = difficulty;

  scene->name = "BattleScene";
  scene->data = data;
  scene->flags = GAME_SCENE_NONE; // 战斗为全屏覆盖层，不依赖下层绘制
  scene->pauseable = false;       // 战斗场景不允许暂停（docs）
  scene->onEnter = BattleEnter;
  scene->onUpdate = BattleUpdate;
  scene->onDraw = BattleDraw;
  scene->onExit = BattleExit;
  // onPause / onResume 本场景不需要，保持 NULL
  return scene;
}
