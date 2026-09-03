#include "game.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// 无尽单词模式（scene_infinite，主菜单 Play 下的 Infinite 入口）：
// 场景风格参考战斗场景，黑色背景 + 网格线条；
// 其实基本上就是没有敌人的战斗场景，只有选择单词的功能：
//   每次给出一题的词性 + 中文释义，玩家从 3 个候选英文单词中选出匹配项
//   （干扰词与答案词性相同、长度相近，保证真正考验对释义的理解）。规则：
//     - 答对：得分 +1、不回复生命值，随即出下一题（答对是“继续”的
//       门票，失误次数即本局资源，失误 6/5/5 次左右（按难度）出局）；
//     - 答错：按难度扣血（easy 15 / normal 20 / hard 25，game_config 集中），
//       进入「答错停留」阶段展示正确答案与释义（复习横幅），玩家看明白后
//       按 Z 进入下一题；HP 归零 → 本局结束，进入本场景自带的结算界面。
//   学习机制（study_tracker）：接入 GameApp 全局错词本/间隔重复抽词 ——
//     1) 题目答案优先复现「间隔到期」的错词（拼错后至少隔
//        STUDY_REVISIT_INTERVAL 题，按拼错次数加权，常错的词更早复现）；
//     2) 其次抽本局未答对的新词；
//     3) 全部答对后回退纯随机。
//   最佳成绩独立持久化：只记录「单局最高答对数」（infiniteBest），读写走
//   save_data 的独立入口，不进速通计时、不与主线 bestTime 互相覆盖。
// 绘制UI（黑底白字，战斗场景风格）：
//   左上角 —— 得分/最佳；顶部中央 —— 总共游戏时间、答错数、答对百分比；
//   右上角 —— ESC 暂停框；左下角 —— 玩家生命值条（HudDrawHealthBar）；
//   中上部 —— 当前题目的中文释义（含词性，长释义按宽度自动折行）；
//   中部 —— 当前题目的三个英文单词选项（键盘 A/D/←/→ 选择、Z 确认）；
//   答错停留 —— 红字 WRONG + 白底复习横幅（正确答案英文 + 词性 + 中文）。
// ─────────────────────────────────────────────────────────────────────────────

// ── 场景常量（布局坐标；数值平衡见 core/game_config.h）─────────────────────
#define INFINITE_GROUND_H 50 // 地面高度（顶面 y=480-50，与各场景一致）
#define INFINITE_OPTION_COUNT 3
#define INFINITE_OPTION_H 46   // 候选单词框高
#define INFINITE_OPTION_GAP 12 // 候选框间距（框宽由 HudLayoutWordRow 按词长自适应）
#define INFINITE_BANNER_MAX_LINES 4 // 复习横幅 / 题目提示最大换行数

// 结算菜单选项（顺序与 MenuNav.selected 索引一一对应）
typedef enum InfiniteOverAction {
  INFINITE_OVER_RETRY = 0, // 以当前难度再来一局
  INFINITE_OVER_MENU,      // 回主菜单
  INFINITE_OVER_QUIT,      // 退出游戏
} InfiniteOverAction;

static const char *const kOverLabels[] = {"Retry", "Back to Menu", "Quit"};
static const InfiniteOverAction kOverActions[] = {
    INFINITE_OVER_RETRY, INFINITE_OVER_MENU, INFINITE_OVER_QUIT};
#define INFINITE_OVER_COUNT 3

// 场景阶段
typedef enum InfinitePhase {
  INFINITE_PHASE_PLAYING = 0, // 玩家可作答
  INFINITE_PHASE_WRONG_HOLD,  // 答错停留：复习正确答案，按 Z 继续
  INFINITE_PHASE_GAMEOVER,    // HP 归零：结算界面
} InfinitePhase;

// 场景私有数据：栈持有并负责释放
typedef struct InfiniteSceneData {
  const GameApp *app; // 只读引用，不拥有
  Player *player;     // 本场景自建并持有（Enter 加载贴图，Exit 卸载释放）
  SceneCamera camera; // 锁定镜头（禁用，世界坐标 == 屏幕坐标）

  int difficulty;         // 难度 0/1/2：词库（CET4/CET6）与拼错惩罚分级
  Rectangle playerSource; // 玩家当前动画帧源矩形

  // 词库与当前三选一题目
  WordsBank bank;
  WordEntry options[INFINITE_OPTION_COUNT]; // 候选（含 1 正确 + 2 干扰）
  int answerIndex;                          // 正确项下标
  int selectIndex;                          // 玩家当前选中的下标
  const WordEntry *answerRef; // 答案在词库内部的指针（StudyMark* 用它定位，
                              // 使错词标记真正落到对应下标，而非失效副本）
  WordEntry answerEntry;      // 答案词条值拷贝（题目提示/复习横幅绘制用，
                              // 词库释放后仍安全）
  char lastWord[64];          // 上一题单词（抽词排除，避免同词连续出现）

  // 玩法进度与成绩
  InfinitePhase phase;
  StudyTracker *study; // 指向 app->study（每局 Enter 重置，跨局独立）
  float elapsed;       // 本局已运行时间（秒，暂停/结算不累计）
  int totalAsked;      // 已作答题数（充当 study.currentLevel：题代关）
  int correctCount;    // 答对数（即得分）
  int wrongCount;      // 答错数
  int streak;          // 当前连续答对
  int bestStreak;      // 本局最长连续答对
  int bestScore;       // 本局已达最佳答对数（HUD 实时更新，结算写入档案）
  bool beatBest;       // 本局是否超越进入时读取的纪录（结算时持久化）
  bool gameOverSoundPlayed;

  float correctFlashTimer; // >0：显示上题答对的绿色反馈（CORRECT!）
  float reviewTimer;       // >0：答错后复习横幅剩余显示时间（最后 1s 淡出）
  WordEntry reviewEntry;   // 复习横幅词条（值拷贝）

  // 结算菜单
  MenuNav overNav;          // 键盘导航（W/S/↑↓ 移动，Z 确认，X 返回菜单）
  int overAction;           // 结算动作：Draw 阶段由按钮写入，Update 消费执行
  bool transitionRequested; // 已请求场景切换，防止同帧重复切换
} InfiniteSceneData;

// ── 工具：UTF-8 字符序列长度 ───────────────────────────────────────────────
// 换行逐「字符」推进时不破坏多字节（中文释义为 UTF-8，逐字节切会乱码）。
static int Utf8SeqLen(const char *s) {
  const unsigned char c = (const unsigned char)*s;
  if (c < 0x80)
    return 1;
  if ((c & 0xE0) == 0xC0)
    return 2;
  if ((c & 0xF0) == 0xE0)
    return 3;
  if ((c & 0xF8) == 0xF0)
    return 4;
  return 1; // 非法首字节：按单字节跳过（防御）
}

// 按显示宽度把文本折成最多 maxLines 行（逐 UTF-8 字符贪心累计，超宽折行；
// 行首空白自动跳过）。out 每行以 '\0' 结尾。返回实际行数（0 表示空文本）。
static int WrapTextLines(const GameApp *app, const char *text, int fontSize,
                         int maxWidth, char out[][300], int maxLines) {
  if (!text || !app || maxLines <= 0)
    return 0;
  int lines = 0;
  char line[300] = {0};
  const char *p = text;
  while (*p && lines < maxLines) {
    const int len = Utf8SeqLen(p);
    // 行首空白跳过
    if (line[0] == '\0' && (*p == ' ' || *p == '\t')) {
      p += len;
      continue;
    }
    // 试拼当前字符到行尾，超宽则先存当前行
    char probe[360];
    snprintf(probe, sizeof(probe), "%s%.*s", line, len, p);
    if (line[0] != '\0' &&
        GameAppMeasureText(app, probe, fontSize) > maxWidth) {
      snprintf(out[lines++], 300, "%s", line);
      line[0] = '\0';
      continue; // 当前字符留到下一行再拼
    }
    // 追加该字符（不破坏 UTF-8）
    const size_t used = strlen(line);
    if (used + (size_t)len < sizeof(line)) {
      memcpy(line + used, p, (size_t)len);
      line[used + (size_t)len] = '\0';
    }
    p += len;
  }
  if (line[0] && lines < maxLines) {
    snprintf(out[lines++], 300, "%s", line);
  }
  return lines;
}

// ── 高质量干扰词 ─────────────────────────────────────────────────────────────
// 与答案词「词性相同」且「长度相近」（|len 差| <= 2），让三选一真正考验对
// 释义的理解。多轮随机采样（词库 7000+，采样 4×40 次足够快）；未命中返回
// NULL，调用方回退纯随机。used[0..usedCount) 为已选干扰词，避免重复。
static const WordEntry *PickQualityDistractor(const WordsBank *bank,
                                              const WordEntry *ans,
                                              const WordEntry *const *used,
                                              int usedCount) {
  if (!bank || bank->count <= 0 || !ans)
    return NULL;
  const int ansLen = (int)strlen(ans->word);
  for (int round = 0; round < 4; round++) {
    for (int i = 0; i < 40; i++) {
      const WordEntry *w = WordsBankPickRandom(bank);
      if (!w || strcmp(w->word, ans->word) == 0)
        continue;
      if (strcmp(w->pos, ans->pos) != 0)
        continue; // 同词性
      const int len = (int)strlen(w->word);
      if (abs(len - ansLen) > 2)
        continue; // 长度相近
      bool dup = false;
      for (int k = 0; k < usedCount; k++) {
        if (used[k] && strcmp(w->word, used[k]->word) == 0) {
          dup = true;
          break;
        }
      }
      if (dup)
        continue;
      return w;
    }
  }
  return NULL;
}

// 生成新一轮三选一：答案词优先走「间隔重复抽词」（到期错词 → 本局未答对
// 新词 → 纯随机，并排除上一题单词），词库为空时用兜底词条（防御，保证场景
// 仍可运行）。干扰词优先「同词性 + 长度相近」，采样不足回退纯随机。
static void SetupChoices(InfiniteSceneData *d) {
  static const WordEntry kFallback[3] = {
      {"cat", "n. 猫", "n."},
      {"dog", "n. 狗", "n."},
      {"run", "v. 跑", "v."},
  };

  // 答案词：绑定错词本时优先 StudyPickWord（学习机制核心，见文件头注释）
  const WordEntry *ans = NULL;
  if (d->study) {
    ans =
        StudyPickWord(d->study, (d->lastWord[0] != '\0') ? d->lastWord : NULL);
  }
  if (!ans)
    ans = WordsBankPickRandom(&d->bank); // 未绑定 / 抽不到 → 纯随机
  if (!ans)
    ans = &kFallback[0]; // 词库为空（防御）
  d->answerRef = ans;
  d->answerEntry = *ans;

  // 两个与答案不同的干扰词：优先高质量（同词性、长度相近），采样不足时
  // 回退纯随机，仍不足用兜底补齐（保证必有 3 个选项）
  const WordEntry *dist[2] = {&kFallback[1], &kFallback[2]};
  int got = 0;
  while (got < 2) {
    const WordEntry *q = PickQualityDistractor(&d->bank, ans, dist, got);
    if (!q)
      break;
    dist[got++] = q;
  }
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

  // 洗牌：答案随机放入一个槽位，其余放干扰词（不足时退化为答案，防御）
  int answerSlot = genRandomNum(INFINITE_OPTION_COUNT);
  d->answerIndex = answerSlot;
  bool used[INFINITE_OPTION_COUNT] = {false, false, false};
  used[answerSlot] = true;
  d->options[answerSlot] = *ans;
  int di = 0;
  for (int i = 0; i < INFINITE_OPTION_COUNT; i++) {
    if (used[i])
      continue;
    const WordEntry *pick = (di < got) ? dist[di++] : ans;
    d->options[i] = *pick;
  }
  d->selectIndex = 0;
  snprintf(d->lastWord, sizeof(d->lastWord), "%s", ans->word);
}

// ── 玩家站立与动画（答题场景不做物理移动，仅更新动画帧）────────────────────
static void UpdatePlayerStanding(InfiniteSceneData *d, float dt) {
  Player *p = d->player;
  if (p->hitTimer > 0.0f) {
    p->hitTimer -= dt;
    if (p->hitTimer < 0.0f)
      p->hitTimer = 0.0f;
    p->playerAnimationState = HIT;
  } else {
    p->playerAnimationState = IDLE;
  }
  d->playerSource =
      AnimationUpdate(&p->animations[p->playerAnimationState], dt);
}

// 答对处理：加分/更新连对与最佳/清错词标记（含真实下标定位）/出新题。
// 设计：答对不回复生命值 —— 失误次数是本局唯一的生命资源（见 game_config
// 无尽段注释），答对只把“下一条命（下一题）”给玩家，并累积得分。
static void HandleCorrect(InfiniteSceneData *d) {
  d->correctCount++;
  d->streak++;
  if (d->streak > d->bestStreak)
    d->bestStreak = d->streak;
  // HUD 实时刷新最佳（最终纪录在结算/结束写入档案）
  if (d->correctCount > d->bestScore) {
    d->bestScore = d->correctCount;
    d->beatBest = true;
  }
  d->correctFlashTimer = INFINITE_CORRECT_FLASH_SECONDS;
  // 学习机制：标记答对（清除该词错词记录与 lastWrong）
  if (d->study)
    StudyMarkCorrect(d->study, d->answerRef);
  // 答对后清除复习横幅（若有残留），进入下一题
  d->reviewTimer = 0.0f;
  SetupChoices(d);
}

// 答错处理：扣血/错词标记/复习横幅/（HP 归零时进入结算）
static void HandleWrong(InfiniteSceneData *d, GameScene *self) {
  static const float kPenalty[3] = {INFINITE_WRONG_PENALTY_EASY,
                                    INFINITE_WRONG_PENALTY_NORMAL,
                                    INFINITE_WRONG_PENALTY_HARD};
  d->wrongCount++;
  d->streak = 0;

  // 扣血：同步受伤检测基准 + 触发 HIT 动画 + 受伤音效（与战斗场景一致）
  const float penalty =
      kPenalty[(d->difficulty >= 0 && d->difficulty < 3) ? d->difficulty : 0];
  Player *p = d->player;
  p->health -= penalty;
  if (p->health < 0.0f)
    p->health = 0.0f;
  p->lastHealth = p->health;
  PlayerTriggerHit(p);
  GameAppPlaySound(d->app, d->app->catHitSound, d->app->catHitSoundValid);

  // 学习机制：标记拼错（wrongCount 加权 + lastWrongLevel = 当前题号，
  // 间隔 STUDY_REVISIT_INTERVAL 题后复现）；复习横幅保存正确答案供学习
  if (d->study)
    StudyMarkWrong(d->study, d->answerRef, d->totalAsked);
  d->reviewEntry = d->answerEntry;
  d->reviewTimer = INFINITE_REVIEW_SECONDS;

  if (p->health <= 0.0f) {
    // HP 归零：进入结算（此时才把本局最佳写回持久化档案）
    d->phase = INFINITE_PHASE_GAMEOVER;
    self->pauseable = false; // 结算界面不再响应 ESC 暂停（ESC 直接回菜单）
    if (d->beatBest)
      SaveDataSaveInfiniteBest(d->bestScore);
    if (!d->gameOverSoundPlayed) {
      GameAppPlaySound(d->app, d->app->gameOverSound,
                       d->app->gameOverSoundValid);
      d->gameOverSoundPlayed = true;
    }
    MenuNavInit(&d->overNav, INFINITE_OVER_COUNT);
    d->overAction = -1;
    d->transitionRequested = false;
    return;
  }
  d->phase = INFINITE_PHASE_WRONG_HOLD;
}

// ── 生命周期 ──────────────────────────────────────────────────────────────

static void InfiniteSceneEnter(GameScene *self) {
  InfiniteSceneData *d = (InfiniteSceneData *)self->data;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 玩家：平台跳跃角色作为答题台下的「站桩角色」（无物理移动，仅动画）
  InitPlayer(d->player);
  d->player->app = d->app; // 注入音频宿主
  PlayerApplyDifficulty(d->player, d->difficulty);
  d->player->health = d->player->maxHealth; // 无尽模式每局从满血开始
  d->player->lastHealth = d->player->health;
  const float groundTop = (float)(screenH - INFINITE_GROUND_H);
  d->player->position = (Vector2){(screenW - d->player->size.x) * 0.5f,
                                  groundTop - d->player->size.y};
  d->player->velocity = (Vector2){0, 0};
  d->player->isOnTheGround = true;
  d->player->playerAnimationState = IDLE;
  d->player->hitTimer = 0.0f;

  // 锁定镜头：禁用相机，固定视野（世界坐标 == 逻辑屏幕坐标）
  InitSceneCamera(&d->camera, screenW, screenH, false, CAMERA_FOLLOW_NONE);
  d->playerSource = AnimationUpdate(&d->player->animations[IDLE], 0.0f);

  // 按难度加载词库（与其它模式一致：简单/普通 CET4，困难 CET6；内嵌资源）
  const char *relPath = "assets/words/CET4.txt";
  if (d->difficulty >= 2)
    relPath = "assets/words/CET6.txt";
  WordsBankLoadEmbedded(&d->bank, relPath);

  // 学习机制：绑定全局错词本并开启新一局（每进一次无尽都是独立一局，
  // 记录不跨局残留；错词复现/新词覆盖逻辑见 systems/study_tracker）
  d->study = NULL;
  if (d->app->study) {
    StudyRebind(d->app->study, &d->bank);
    StudyReset(d->app->study);
    d->app->study->currentLevel = 0;
    d->study = d->app->study;
  }

  // 状态清零：最佳纪录从持久化档案读取
  d->phase = INFINITE_PHASE_PLAYING;
  d->elapsed = 0.0f;
  d->totalAsked = 0;
  d->correctCount = 0;
  d->wrongCount = 0;
  d->streak = 0;
  d->bestStreak = 0;
  d->bestScore = SaveDataLoadInfiniteBest();
  d->beatBest = false;
  d->gameOverSoundPlayed = false;
  d->correctFlashTimer = 0.0f;
  d->reviewTimer = 0.0f;
  d->lastWord[0] = '\0';
  d->overAction = -1;
  d->transitionRequested = false;
  SetupChoices(d);
}

// 作答中更新：选项移动（A/D 或 ←/→，循环）+ Z 确认
static void UpdatePlaying(GameScene *self, float dt) {
  InfiniteSceneData *d = (InfiniteSceneData *)self->data;
  (void)dt;
  const int prev = d->selectIndex;
  if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) {
    d->selectIndex =
        (d->selectIndex + INFINITE_OPTION_COUNT - 1) % INFINITE_OPTION_COUNT;
  } else if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) {
    d->selectIndex = (d->selectIndex + 1) % INFINITE_OPTION_COUNT;
  }
  if (d->selectIndex != prev) {
    GameAppPlaySound(d->app, d->app->uiSound, d->app->uiSoundValid);
  }
  if (!IsKeyPressed(KEY_Z))
    return;

  // 确认：先播选择音，再计题号（study.currentLevel 以「题」代「关」）
  GameAppPlaySound(d->app, d->app->uiSound, d->app->uiSoundValid);
  d->totalAsked++;
  if (d->study)
    d->study->currentLevel = d->totalAsked;

  if (d->selectIndex == d->answerIndex) {
    HandleCorrect(d);
  } else {
    HandleWrong(d, self);
  }
}

// 答错停留：玩家复习正确答案；按 Z 出下一题（X/ESC 走暂停或返回菜单）
static void UpdateWrongHold(InfiniteSceneData *d) {
  if (!IsKeyPressed(KEY_Z))
    return;
  GameAppPlaySound(d->app, d->app->uiSound, d->app->uiSoundValid);
  d->reviewTimer = 0.0f; // 已复习，清除横幅
  d->phase = INFINITE_PHASE_PLAYING;
  SetupChoices(d);
}

// 结算更新：键盘导航 / 鼠标按钮；动作统一在此执行
static void UpdateGameOver(GameScene *self) {
  InfiniteSceneData *d = (InfiniteSceneData *)self->data;

  // ESC / X：返回主菜单（等价于 Back to Menu）
  if (IsKeyPressed(KEY_ESCAPE)) {
    if (!d->transitionRequested) {
      d->transitionRequested = true;
      GameStackClearTo(self->owner, StartSceneCreate((GameApp *)d->app));
    }
    return;
  }
  const int prev = d->overNav.selected;
  MenuAction act = MenuNavUpdate(&d->overNav);
  if (d->overNav.selected != prev || act != MENU_ACTION_NONE) {
    GameAppPlaySound(d->app, d->app->uiSound, d->app->uiSoundValid);
  }
  if (act == MENU_ACTION_BACK) {
    if (!d->transitionRequested) {
      d->transitionRequested = true;
      GameStackClearTo(self->owner, StartSceneCreate((GameApp *)d->app));
    }
    return;
  }
  if (act == MENU_ACTION_CONFIRM) {
    d->overAction = (int)kOverActions[d->overNav.selected];
  }

  switch (d->overAction) {
  case INFINITE_OVER_RETRY:
    if (!d->transitionRequested) {
      d->transitionRequested = true;
      GameStackReplace(self->owner,
                       TransitionSceneCreate(
                           d->app, InfiniteSceneCreate(d->app, d->difficulty)));
    }
    break;
  case INFINITE_OVER_MENU:
    if (!d->transitionRequested) {
      d->transitionRequested = true;
      GameStackClearTo(self->owner, StartSceneCreate((GameApp *)d->app));
    }
    break;
  case INFINITE_OVER_QUIT:
    if (!d->transitionRequested) {
      d->transitionRequested = true;
      GameStackRequestQuit(self->owner);
    }
    break;
  default:
    break;
  }
  d->overAction = -1;
}

static void InfiniteSceneUpdate(GameScene *self, float dt) {
  InfiniteSceneData *d = (InfiniteSceneData *)self->data;

  switch (d->phase) {
  case INFINITE_PHASE_PLAYING:
    UpdatePlaying(self, dt);
    break;
  case INFINITE_PHASE_WRONG_HOLD:
    UpdateWrongHold(d);
    break;
  case INFINITE_PHASE_GAMEOVER:
    UpdateGameOver(self);
    break;
  default:
    break;
  }

  // 反馈/复习横幅计时（暂停时 dt=0 自然冻结）
  if (d->correctFlashTimer > 0.0f) {
    d->correctFlashTimer -= dt;
    if (d->correctFlashTimer < 0.0f)
      d->correctFlashTimer = 0.0f;
  }
  if (d->reviewTimer > 0.0f) {
    d->reviewTimer -= dt;
    if (d->reviewTimer < 0.0f)
      d->reviewTimer = 0.0f;
  }
  // 本局运行时间 + 玩家动画（结算界面静止不累计、不更新）
  if (d->phase != INFINITE_PHASE_GAMEOVER) {
    d->elapsed += dt;
    UpdatePlayerStanding(d, dt);
  }
}

// ── 绘制 ─────────────────────────────────────────────────────────────────

// 答对率（百分比，未答题时返回 0 供调用方显示 "--"）
static int InfiniteAccuracy(const InfiniteSceneData *d) {
  if (d->totalAsked <= 0)
    return 0;
  return (d->correctCount * 100) / d->totalAsked;
}

// 顶部统计栏（黑底白字，战斗场景风格，三列）：
//   左列：Score（答对数）/ Best（纪录）；中列：本局时间 / 答错数·答对率；
//   右列：ESC 暂停框（自绘浅色版，黑底上 HudDrawEscHint 的深框不可见）。
static void DrawTopBar(InfiniteSceneData *d) {
  const int screenW = d->app->logicWidth;
  const int fs = 16;
  char text[64];

  // 左列：得分 / 最佳
  snprintf(text, sizeof(text), "Score : %d", d->correctCount);
  GameAppDrawText(d->app, text, 12, 12, fs, WHITE);
  snprintf(text, sizeof(text), "Best : %d", d->bestScore);
  GameAppDrawText(d->app, text, 12, 34, fs, LIGHTGRAY);

  // 中列：本局时间 / 答错数 + 答对率（未答题时显示 --）
  const int totalSec = (int)d->elapsed;
  snprintf(text, sizeof(text), "Time %02d:%02d", totalSec / 60,
           totalSec % 60);
  GameAppDrawText(d->app, text,
                  (screenW - GameAppMeasureText(d->app, text, fs)) / 2, 12,
                  fs, LIGHTGRAY);
  if (d->totalAsked > 0) {
    snprintf(text, sizeof(text), "Wrong %d    Acc %d%%", d->wrongCount,
             InfiniteAccuracy(d));
  } else {
    snprintf(text, sizeof(text), "Wrong %d    Acc --", d->wrongCount);
  }
  GameAppDrawText(d->app, text,
                  (screenW - GameAppMeasureText(d->app, text, fs)) / 2, 34,
                  fs, LIGHTGRAY);

  // 右列：ESC 暂停提示框（黑底上使用浅色描边）
  const int escW = GameAppMeasureText(d->app, "ESC", fs);
  const int boxW = escW + 18;
  const int boxH = fs + 10;
  const int boxX = screenW - 12 - boxW;
  const int boxY = 12;
  DrawRectangle(boxX, boxY, boxW, boxH, Fade(WHITE, 0.10f));
  DrawRectangleLines(boxX, boxY, boxW, boxH, GRAY);
  GameAppDrawText(d->app, "ESC", boxX + (boxW - escW) / 2,
                  boxY + (boxH - fs) / 2, fs, LIGHTGRAY);
}

// 题目提示（答案的词性 + 中文释义：蓝色字体，黑底上清晰醒目；
// 按宽度折行居中，最多 4 行）
static void DrawQuestionHint(InfiniteSceneData *d) {
  char hint[384];
  snprintf(hint, sizeof(hint), "%s %s", d->answerEntry.pos,
           d->answerEntry.meaning);
  const int fs = 18;
  const int maxW = d->app->logicWidth - 48;
  char lines[INFINITE_BANNER_MAX_LINES][300];
  const int n =
      WrapTextLines(d->app, hint, fs, maxW, lines, INFINITE_BANNER_MAX_LINES);
  const int y0 = 64; // 顶部统计两行之下；最多 4 行，末尾行底约 y=148 < 选项
  for (int i = 0; i < n; i++) {
    const int w = GameAppMeasureText(d->app, lines[i], fs);
    GameAppDrawText(d->app, lines[i], (d->app->logicWidth - w) / 2,
                    y0 + i * (fs + 4), fs, SKYBLUE);
  }
}

// 三个候选英文单词框（居中一行，黑底风格：白字浅框、选中黄框高亮）。
// 框宽随单词长度自适应（HudLayoutWordRow：词长越长框越宽，避免长词溢出
// 与相邻框/文字重叠）；答错停留时把答案框描绿，其余选项淡化。
static void DrawOptions(InfiniteSceneData *d) {
  const int screenW = d->app->logicWidth;
  const int boxY = 158; // 题目提示（≤4 行）之下、WRONG/复习横幅之上

  // 自适应行布局：框宽随词长浮动，总宽放不下时整行统一缩字号再试
  const char *words[INFINITE_OPTION_COUNT];
  for (int i = 0; i < INFINITE_OPTION_COUNT; i++)
    words[i] = d->options[i].word;
  Rectangle rects[INFINITE_OPTION_COUNT];
  const int fs = HudLayoutWordRow(
      d->app, words, INFINITE_OPTION_COUNT, (float)screenW,
      INFINITE_OPTION_GAP, 14 /*padX*/, 16 /*base*/, 10 /*min*/, 110.0f,
      (float)INFINITE_OPTION_H, (float)boxY, rects);

  for (int i = 0; i < INFINITE_OPTION_COUNT; i++) {
    Rectangle rec = rects[i];
    const bool selected = (i == d->selectIndex);
    const bool isAnswer = (i == d->answerIndex);
    // 黑底上沿用战斗场景玩家回合的配色：未选白底淡显、选中黄框高亮
    Color border = GRAY;
    Color fill = Fade(WHITE, 0.06f);
    Color textCol = LIGHTGRAY;
    if (selected) {
      border = YELLOW;
      fill = Fade(WHITE, 0.18f);
      textCol = WHITE;
    }
    if (d->phase == INFINITE_PHASE_WRONG_HOLD) {
      if (isAnswer) {
        border = GREEN;
        fill = Fade(GREEN, 0.16f); // 正确答案框：绿框提示
        textCol = WHITE;
      } else if (!selected) {
        fill = Fade(WHITE, 0.02f); // 非答案框淡化，凸显正确项
      }
    }
    DrawRectangleRec(rec, fill);
    DrawRectangleLinesEx(rec, selected || isAnswer ? 2.0f : 1.0f, border);

    const char *word = d->options[i].word;
    const int tw = GameAppMeasureText(d->app, word, fs);
    GameAppDrawText(d->app, word, (int)(rec.x + rec.width * 0.5f) - tw / 2,
                    (int)(rec.y + rec.height * 0.5f) - fs / 2, fs, textCol);
  }
}

// 复习横幅（答错停留）：白底绿框黑字，显示正确答案与词性/释义。
// 内容按宽度折行（最长 4 行），随剩余时间最后 1 秒淡出。
static void DrawReviewBanner(InfiniteSceneData *d) {
  if (d->phase != INFINITE_PHASE_WRONG_HOLD || d->reviewTimer <= 0.0f)
    return;
  char text[384];
  snprintf(text, sizeof(text), "正确：%s  (%s %s)", d->reviewEntry.word,
           d->reviewEntry.pos, d->reviewEntry.meaning);
  const int fs = 18;
  const int maxW = d->app->logicWidth - 80;
  char lines[INFINITE_BANNER_MAX_LINES][300];
  const int n =
      WrapTextLines(d->app, text, fs, maxW, lines, INFINITE_BANNER_MAX_LINES);
  if (n <= 0)
    return;

  // 横幅宽度按最宽行自适应
  int lineW = 0;
  for (int i = 0; i < n; i++) {
    const int w = GameAppMeasureText(d->app, lines[i], fs);
    if (w > lineW)
      lineW = w;
  }
  const float padX = 16.0f;
  const float padY = 12.0f;
  const float bw = (float)lineW + padX * 2.0f;
  const float bh = (float)n * ((float)fs + 4.0f) + padY * 2.0f - 4.0f;
  const float bx = ((float)d->app->logicWidth - bw) * 0.5f;
  const float by = 252.0f; // WRONG 大字之下，最长 4 行底约 y=336 < 玩家头顶
  const float alpha =
      (d->reviewTimer < 1.0f) ? d->reviewTimer : 1.0f; // 最后 1s 淡出
  DrawRectangle((int)bx, (int)by, (int)bw, (int)bh, Fade(WHITE, alpha * 0.95f));
  DrawRectangleLines((int)bx, (int)by, (int)bw, (int)bh, Fade(GREEN, alpha));
  for (int i = 0; i < n; i++) {
    const int w = GameAppMeasureText(d->app, lines[i], fs);
    GameAppDrawText(d->app, lines[i], (int)(bx + (bw - (float)w) * 0.5f),
                    (int)(by + padY) + i * (fs + 4), fs, Fade(BLACK, alpha));
  }
}

// 作答画面 HUD（不含世界层）：题目/选项/反馈/横幅/底部提示
static void DrawAnswerHud(InfiniteSceneData *d) {
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 左下角：玩家生命值条（黑底上与战斗场景一致，raygui 深色样式可读）
  HudDrawHealthBar(d->app, d->player->health, d->player->maxHealth);

  DrawTopBar(d);
  DrawQuestionHint(d);
  DrawOptions(d);

  if (d->phase == INFINITE_PHASE_PLAYING) {
    // 答对反馈（短时绿色横幅，出现在选项下方）
    if (d->correctFlashTimer > 0.0f) {
      const int fs = 22;
      const char *flash = TextFormat("CORRECT!  Streak %d", d->streak);
      GameAppDrawText(d->app, flash,
                      (screenW - GameAppMeasureText(d->app, flash, fs)) / 2,
                      218, fs, GREEN);
    }
    const char *help = "Move: A/D or Arrows    Confirm: Z";
    GameAppDrawText(d->app, help,
                    screenW - 12 - GameAppMeasureText(d->app, help, 16),
                    screenH - 24, 16, LIGHTGRAY);
  } else if (d->phase == INFINITE_PHASE_WRONG_HOLD) {
    const int fs = 26;
    const char *wrong = "WRONG!";
    GameAppDrawText(d->app, wrong,
                    (screenW - GameAppMeasureText(d->app, wrong, fs)) / 2, 214,
                    fs, RED);
    DrawReviewBanner(d);
    const char *help = "Confirm: Z to next word";
    GameAppDrawText(d->app, help,
                    screenW - 12 - GameAppMeasureText(d->app, help, 16),
                    screenH - 24, 16, LIGHTGRAY);
  }
}

// 世界层（战斗场景风格）：黑色背景 + 浅灰网格线，底部地面，站桩玩家
static void DrawStage(InfiniteSceneData *d) {
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  BeginSceneCamera(&d->camera);
  // 黑色背景（覆盖渲染目标默认的 RAYWHITE 清屏）+ 网格线（与战斗一致）
  DrawRectangle(0, 0, screenW, screenH, BLACK);
  const int grid = 40;
  for (int x = 0; x <= screenW; x += grid)
    DrawLine(x, 0, x, screenH, Fade(LIGHTGRAY, 0.10f));
  for (int y = 0; y <= screenH; y += grid)
    DrawLine(0, y, screenW, y, Fade(LIGHTGRAY, 0.10f));

  // 地面（顶面 480-50，与各场景一致）
  DrawRectangle(0, screenH - INFINITE_GROUND_H, screenW, INFINITE_GROUND_H,
                Fade(LIGHTGRAY, 0.30f));
  DrawLine(0, screenH - INFINITE_GROUND_H, screenW, screenH - INFINITE_GROUND_H,
           Fade(LIGHTGRAY, 0.60f));
  DrawPlayer(d->player, d->playerSource);
  EndSceneCamera(&d->camera);
}

// 结算界面：盖在答题画面上的全屏结算层
static void DrawGameOver(InfiniteSceneData *d) {
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.88f));

  const char *title = "GAME OVER";
  const int titleSize = 44;
  GameAppDrawText(d->app, title,
                  (screenW - GameAppMeasureText(d->app, title, titleSize)) / 2,
                  screenH / 4 - titleSize / 2, titleSize, RED);

  const char *subtitle = "Your HP reached 0!";
  const int subSize = 18;
  GameAppDrawText(d->app, subtitle,
                  (screenW - GameAppMeasureText(d->app, subtitle, subSize)) / 2,
                  screenH / 4 + titleSize / 2 + 10, subSize, LIGHTGRAY);

  // 本局成绩统计（首行已含时间与答对数，次行纪录，末行错/答对率/连对）
  const int statSize = 16;
  char stat[128];
  const int totalSec = (int)d->elapsed;
  snprintf(stat, sizeof(stat), "Correct : %d        Time %02d:%02d",
           d->correctCount, totalSec / 60, totalSec % 60);
  GameAppDrawText(d->app, stat,
                  (screenW - GameAppMeasureText(d->app, stat, statSize)) / 2,
                  178, statSize, WHITE);
  if (d->beatBest && d->correctCount > 0) {
    const int recSize = 22;
    const char *rec = TextFormat("NEW BEST : %d words!", d->bestScore);
    GameAppDrawText(d->app, rec,
                    (screenW - GameAppMeasureText(d->app, rec, recSize)) / 2,
                    210, recSize, GOLD);
  } else {
    snprintf(stat, sizeof(stat), "Best : %d", d->bestScore);
    GameAppDrawText(d->app, stat,
                    (screenW - GameAppMeasureText(d->app, stat, statSize)) / 2,
                    210, statSize, GOLD);
  }
  if (d->totalAsked > 0) {
    snprintf(stat, sizeof(stat), "Wrong : %d     Acc : %d%%     Best Streak : %d",
             d->wrongCount, InfiniteAccuracy(d), d->bestStreak);
  } else {
    snprintf(stat, sizeof(stat), "Wrong : %d     Acc : --     Best Streak : %d",
             d->wrongCount, d->bestStreak);
  }
  GameAppDrawText(d->app, stat,
                  (screenW - GameAppMeasureText(d->app, stat, statSize)) / 2,
                  242, statSize, LIGHTGRAY);

  // 结算按钮（Retry / Back to Menu / Quit）
  const float btnW = 210;
  const float btnH = 40;
  const float btnX = (screenW - btnW) / 2;
  const float btnY = screenH / 2.f + 40;
  const float gap = 10;

  for (int i = 0; i < INFINITE_OVER_COUNT; i++) {
    Rectangle rec = {
        .x = btnX, .y = btnY + i * (btnH + gap), .width = btnW, .height = btnH};
    if (CheckCollisionPointRec(GetMousePosition(), rec)) {
      d->overNav.selected = i;
    }
    if (i == d->overNav.selected) {
      GuiSetState(STATE_FOCUSED);
    }
    bool clicked = GuiButton(rec, kOverLabels[i]);
    if (i == d->overNav.selected) {
      GuiSetState(STATE_NORMAL);
    }
    if (clicked) {
      d->overAction = (int)kOverActions[i];
    }
  }

  const char *hint = "Move: W/S or Arrows    Confirm: Z    Back: X / ESC";
  GameAppDrawText(d->app, hint,
                  (screenW - GameAppMeasureText(d->app, hint, 16)) / 2,
                  screenH - 24, 16, LIGHTGRAY);
}

static void InfiniteSceneDraw(GameScene *self) {
  InfiniteSceneData *d = (InfiniteSceneData *)self->data;
  DrawStage(d);
  if (d->phase == INFINITE_PHASE_GAMEOVER) {
    DrawGameOver(d);
  } else {
    DrawAnswerHud(d);
  }
}

static void InfiniteSceneExit(GameScene *self) {
  InfiniteSceneData *d = (InfiniteSceneData *)self->data;

  // 释放词库（study 指向它的指针随之失效；进入新局时 StudyRebind +
  // StudyReset 会重新指向新词库并清空记录，不会读旧指针）
  WordsBankFree(&d->bank);

  // 释放玩家（含 5 张贴图，与 InitPlayer 一一配对）
  if (d->player) {
    UnloadTexture(d->player->idleTexture);
    UnloadTexture(d->player->runTexture);
    UnloadTexture(d->player->jumpTexture);
    UnloadTexture(d->player->sleepTexture);
    UnloadTexture(d->player->hitTexture);
    free(d->player);
    d->player = NULL;
  }
}

GameScene *InfiniteSceneCreate(const GameApp *app, int difficulty) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  if (scene == NULL) {
    return NULL;
  }
  InfiniteSceneData *data =
      (InfiniteSceneData *)calloc(1, sizeof(InfiniteSceneData));
  if (data == NULL) {
    free(scene);
    return NULL;
  }
  data->player = (Player *)calloc(1, sizeof(Player));
  if (data->player == NULL) {
    free(data);
    free(scene);
    return NULL;
  }

  data->app = app;
  data->difficulty = difficulty;
  data->overAction = -1;

  scene->name = "InfiniteScene";
  scene->data = data;
  scene->flags = GAME_SCENE_DRAW_WHEN_HIDDEN; // 被暂停覆盖层遮挡时仍绘制
  scene->pauseable = true; // 无尽模式允许暂停（答题间隙/错题停留均可）
  scene->onEnter = InfiniteSceneEnter;
  scene->onUpdate = InfiniteSceneUpdate;
  scene->onDraw = InfiniteSceneDraw;
  scene->onExit = InfiniteSceneExit;
  // onPause / onResume 本场景不需要，保持 NULL
  return scene;
}
