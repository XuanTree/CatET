#include "scenes/scene_spell.h"
#include "game.h"
#include <stdlib.h>
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// 极速拼写关卡（docs/game_instructions.md 关卡设计 1）：
//   - 限时 40 秒（HUD 右下角倒计时），固定摄像机镜头（锁定视野，世界 ==
//   屏幕）。
//   - 用 LARGE 大平台作为核心落脚点，不额外绘制矩形地面（docs 关卡设计）。
//   - 单词随机挖空多个字母（按难度 2~4 空，短词自动收敛），附词性与中文释义
//     提示；玩家可 Z 键拾取/放下字母（复用 Character 组件）。
//   - 每次天上落下一批字母：包含当前所有未填挖空的字母（正确候选）+ 若干干扰
//     字母；玩家把任一正确字母带到拼写平台（大平台中央绿色高亮区）放下，即
//     填掉一个挖空，并清空平台上其余字母、掉落下一批；全部挖空填满即通关。
//   - 拼写错误扣血并重置填空（生成新词 + 重新掉落字母，倒计时不重置）；
//     倒计时归零扣血并进入下一关；HP 归零判定失败。
// ─────────────────────────────────────────────────────────────────────────────

// ── 关卡常量 ──────────────────────────────────────────────────────────────
#define SPELL_TIME_LIMIT                                                       \
  15.0f                          // 极速拼写关卡限时 15 秒（超时/拼错/掉落惩罚
                                 // 统一用 TIME_PENALTY / SPELL_WRONG_PENALTY /
                                 // FALL_PENALTY_RATIO，见 core/game_config.h）
#define SPELL_FALL_SPEED 150.0f  // 字母下落速度（世界坐标/秒）
#define SPELL_DISTRACTORS 2      // 每批干扰字母数（与剩余正确字母一同掉落）
#define SPELL_MAX_FALL_LETTERS 8 // 空中下落字母数组上限
#define SPELL_PLAT_TOP 400.0f    // 大平台可见顶面目标 y（世界坐标）
#define SPELL_DROP_PAD 30.0f     // 字母出生/落点距平台边缘的最小间距

// 天上落下的字母：下落中 → 落地后并入 character.letters（由 Character 组件
// 负责拾取/拼写判定），merged 标记避免重复绘制/重复并入。
typedef struct SpellFallingLetter {
  char ch;        // 字母字符
  bool isCorrect; // 是否为所需正确字母
  Vector2 pos;    // 圆心位置（世界坐标）
  bool falling;   // 是否仍在空中下落
  bool merged;    // 是否已并入 character.letters
} SpellFallingLetter;

typedef struct SpellSceneData {
  const GameApp *app;
  Player *cat;
  Platform *large_platform;
  Character character;
  SceneCamera
      camera;    // 锁定镜头（禁用相机，固定视野：世界坐标 == 逻辑屏幕坐标）
  Rectangle rec; // 玩家当前动画帧源矩形

  // Data
  float timeLeft;
  int lastTickSecond; // 最近一次 tick 的剩余整秒刻度（0=未提示，见 timer.h）
  int difficulty;
  int level;

  // 场景运行状态
  GameStack *owner;         // 所属栈（供拼写事件切换场景）
  bool transitionRequested; // 已请求场景切换，防止同帧重复切换
  SpellFallingLetter falling[SPELL_MAX_FALL_LETTERS]; // 天上落下的字母
  int fallingCount;
} SpellSceneData;

// ── 前向声明 ──────────────────────────────────────────────────────────────
// 供 SpellSceneEnter 注入 Character 组件的回调（定义在本文件下方）。
static Vector2 SpellDropResolver(void *ctx, const Player *p);
static void SpellOnSpellCorrect(void *ctx);
static void SpellOnSpellWrong(void *ctx);

// ── 工具函数
// ──────────────────────────────────────────────────────────────────

// 大平台可见顶面 y（世界坐标）：贴图左上角 + 顶部透明留白
static float SpellPlatformTop(const SpellSceneData *d) {
  return d->large_platform->spawnPosition.y + d->large_platform->surfaceOffset;
}

// 随机干扰字母：必须不在单词中（既非剩余挖空字母，也非已填/可见字母），
// 避免玩家误以为某个「单词已含字母」也是待填项而误判为正确。
static char SpellRandomDistractor(const SpellSceneData *d) {
  char ch;
  do {
    ch = (char)('a' + genRandomNum(26));
  } while (strchr(d->character.entry.word, ch) != NULL);
  return ch;
}

// 在 [minX, maxX] 内生成 count 个互不重叠的 x 落点：先预留相邻最小间距
// （pickupRadius×2 + 余量），再把整组字母随机整体平移，保证字母不堆叠、
// 玩家一眼可分清；空间不足时退化为尽量均匀分布。
static void SpellSpreadXs(SpellSceneData *d, float *xs, int count, float minX,
                          float maxX) {
  const float span = maxX - minX;
  if (span <= 0.0f) {
    for (int i = 0; i < count; i++)
      xs[i] = minX;
    return;
  }
  if (count <= 1) {
    xs[0] = minX + (float)genRandomNum((int)(span + 1));
    return;
  }
  const float gap = d->character.pickupRadius * 2.0f + 6.0f; // 相邻最小间距
  const float needed = gap * (float)(count - 1);
  const float extra = span - needed; // 可整体平移的余量
  const float start =
      minX + (extra > 0.0f ? (float)genRandomNum((int)(extra + 1)) : 0.0f);
  for (int i = 0; i < count; i++)
    xs[i] = start + gap * (float)i;
  // 空间不足（放不下完整间距）：回退为尽量均匀分布
  if (xs[count - 1] > maxX) {
    for (int i = 0; i < count; i++)
      xs[i] = minX + span * ((float)i / (float)(count - 1));
  }
}

// 掉落一批字母：包含「当前仍未填写的全部挖空字母」（去重）作为正确候选 +
// SPELL_DISTRACTORS 个干扰字母。玩家正确选择其中任一挖空字母后，场景会清空
// 平台并掉落下一批（见 SpellOnSpellCorrect）；全部挖空填满即通关。
// 出生 x 限定在大平台水平范围内，保证必然落到平台上；出生 y 在屏幕上方。
// 落点 x 经 SpellSpreadXs 分散，避免字母重叠难以辨认。
static void SpellSpawnWave(SpellSceneData *d) {
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
    return; // 无剩余挖空（防御，正常应已通关）

  // 本批字母：全部剩余正确字母 + 干扰字母（防空位）
  int count = answerCount + SPELL_DISTRACTORS;
  if (count > SPELL_MAX_FALL_LETTERS - d->fallingCount)
    count = SPELL_MAX_FALL_LETTERS - d->fallingCount;
  if (count < 1)
    return;

  const Platform *p = d->large_platform;
  const float minX = p->spawnPosition.x + SPELL_DROP_PAD;
  const float maxX = p->spawnPosition.x + p->size.x - SPELL_DROP_PAD;

  // 生成互不重叠的落点 x（避免字母堆叠）
  float xs[SPELL_MAX_FALL_LETTERS];
  SpellSpreadXs(d, xs, count, minX, maxX);

  for (int i = 0; i < count; i++) {
    SpellFallingLetter *f = &d->falling[d->fallingCount++];
    f->isCorrect = (i < answerCount);
    f->ch = f->isCorrect ? answers[i] : SpellRandomDistractor(d);
    f->pos = (Vector2){xs[i], -30.0f};
    f->falling = true;
    f->merged = false;
  }
}

// 字母落地后并入 Character 组件：letterCount 满时移除一个未被拾取的旧字母
// 为新字母腾出位置（避免平台上字母无限堆积）。
static void SpellMergeLanded(SpellSceneData *d, SpellFallingLetter *f) {
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
  } else {
    // 防御：平台仍满则悬停在顶面，下帧再试并入
    f->pos.y = SpellPlatformTop(d) - c->pickupRadius;
  }
}

// 下落字母更新：下落 → 撞到大平台顶面落地并入 character；已落地但平台满
// 未并入的字母每帧重试并入（玩家拾取腾出位置后即可并入）。
static void SpellUpdateFalling(SpellSceneData *d, float dt) {
  const float landY = SpellPlatformTop(d) - d->character.pickupRadius;
  for (int i = 0; i < d->fallingCount; i++) {
    SpellFallingLetter *f = &d->falling[i];
    if (f->merged)
      continue;
    if (f->falling) {
      f->pos.y += SPELL_FALL_SPEED * dt;
      if (f->pos.y >= landY) {
        f->pos.y = landY;
        f->falling = false;
      }
      // 仍在空中：不并入 character，让字母从天上完整落到平台顶面。
      // （修复 BUG：若下落途中就并入，字母会立刻被标记 merged 并冻结在
      //  出生点的屏幕外位置，导致玩家看不到任何掉落字母、无法通关。）
      continue;
    }
    // 已落地（含平台满未并入）：尝试并入 character
    SpellMergeLanded(d, f);
  }
}

// 玩家不设水平空气墙：允许走出大平台边缘自然掉落；掉出屏幕底部则回大
// 平台顶面并扣除生命值（掉落惩罚，与平台跳跃关卡一致——通过操作防止掉
// 出屏幕也是玩法的一环）。
static void SpellClampPlayer(SpellSceneData *d) {
  Player *p = d->cat;
  const Platform *plat = d->large_platform;
  const float platTop = SpellPlatformTop(d);

  // 掉出屏幕底部（含走出平台边缘后的坠落）：回平台顶面并扣 20% 最大生命值
  if (p->position.y > d->app->logicHeight + 100.0f) {
    p->position.x =
        plat->spawnPosition.x + plat->size.x * 0.5f - p->size.x * 0.5f;
    p->position.y = platTop - p->size.y;
    p->velocity = (Vector2){0, 0};
    p->isOnTheGround = true;
    p->health -= p->maxHealth * FALL_PENALTY_RATIO; // 掉落惩罚
    if (p->health < 0.0f)
      p->health = 0.0f;
  }
}

// 重置填空：生成新谜题、清空平台字母与空中字母、重新掉落一批（倒计时不重置）
static void SpellResetPuzzle(SpellSceneData *d) {
  CharacterSetupPuzzle(&d->character);
  d->character.letterCount = 0;
  d->character.holdingLetter = false;
  d->character.heldLetterIndex = -1;
  for (int i = 0; i < CHARACTER_MAX_LETTERS; i++)
    d->character.letters[i].isPickedUp = false;
  d->fallingCount = 0;
  SpellSpawnWave(d);
}

// 通关：最终胜利（第 MAX_LEVELS
// 关）或普通通关进入下一关（经转场，只请求一次）。
// 仅由拼写正确（填满全部挖空）触发；超时已改为扣血留在本关（见
// SpellSceneUpdate，2026-08 修正「躺过」漏洞）。
static void SpellAdvanceNext(SpellSceneData *d) {
  if (d->transitionRequested)
    return;
  d->transitionRequested = true;
  if (d->level >= MAX_LEVELS) {
    // 最终通关：记录速通最佳时间，经过渡进入通关结算场景
    // （scene_finish，最终胜利音效由该场景 onEnter 播放）
    SpeedrunFinish((GameApp *)d->app);
    GameStackReplace(d->owner,
                     TransitionSceneCreate(d->app, FinishSceneCreate(d->app)));
    return;
  }
  // 普通通关：播放通关单关音效，经过渡进入下一关（类型按 level_flow 权重刷新）
  GameAppPlaySound(d->app, d->app->levelFinishSound,
                   d->app->levelFinishSoundValid);
  GameStackReplace(d->owner, TransitionSceneCreate(
                                 d->app, LevelFlowCreateNextScene(
                                             d->app, d->level, d->difficulty)));
}

// ── Character 组件回调
// ─────────────────────────────────────────────────────────

// 拼写正确（填掉一个挖空）：
//   - 若仍有未填写的挖空：清空平台上其余字母并掉落下一批（玩家继续填空）；
//   - 若全部挖空已填满：通关进入下一关。
static void SpellOnSpellCorrect(void *ctx) {
  SpellSceneData *d = (SpellSceneData *)ctx;
  if (CharacterRemainingBlanks(&d->character) == 0) {
    // 全部挖空填满 → 通关：发放通关奖励（随关卡递增，上限最大生命值）
    PlayerHeal(d->cat, ClearHealthReward(d->level));
    SpellAdvanceNext(d);
    return;
  }
  // 还有挖空：清空平台上其余字母（含未拾取的干扰/多余字母）+ 掉落下一批
  d->character.letterCount = 0;
  d->character.holdingLetter = false;
  d->character.heldLetterIndex = -1;
  d->fallingCount = 0;
  SpellSpawnWave(d);
}

// 拼写错误：扣血并重置填空（docs：扣除一定生命值并重置填空，计时器不重置）
static void SpellOnSpellWrong(void *ctx) {
  SpellSceneData *d = (SpellSceneData *)ctx;
  d->cat->health -= SPELL_WRONG_PENALTY;
  if (d->cat->health < 0.0f)
    d->cat->health = 0.0f;
  SpellResetPuzzle(d);
}

// 放下落点解析（Character 回调）：字母落在大平台顶面（水平按玩家中心钳制，
// 保证任意位置放下都不悬空、可再拾取）
static Vector2 SpellDropResolver(void *ctx, const Player *p) {
  SpellSceneData *d = (SpellSceneData *)ctx;
  float x = p->position.x + p->size.x * 0.5f;
  const float left = d->large_platform->spawnPosition.x;
  const float right =
      d->large_platform->spawnPosition.x + d->large_platform->size.x;
  if (x < left)
    x = left;
  else if (x > right)
    x = right;
  return (Vector2){x, SpellPlatformTop(d) - d->character.pickupRadius};
}

// ── 生命周期 ──────────────────────────────────────────────────────────────

static void SpellSceneEnter(GameScene *self) {
  SpellSceneData *d = (SpellSceneData *)self->data;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 玩家：平台跳跃物理，出生在大平台顶面中央
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

  // 大平台：核心落脚点（无须额外绘制矩形地面，docs 关卡设计）
  InitJumpPlatforms(d->large_platform, (Vector2){0, 0}, LARGE);
  const float platTop = SPELL_PLAT_TOP;
  d->large_platform->spawnPosition.x =
      (screenW - d->large_platform->size.x) * 0.5f;
  d->large_platform->spawnPosition.y =
      platTop - d->large_platform->surfaceOffset;

  // 玩家站到平台顶面中央
  d->cat->position =
      (Vector2){(screenW - d->cat->size.x) * 0.5f, platTop - d->cat->size.y};
  d->cat->velocity = (Vector2){0, 0};
  d->cat->isOnTheGround = true;

  // 锁定镜头：禁用相机，固定视野（世界坐标 == 逻辑屏幕坐标）
  InitSceneCamera(&d->camera, screenW, screenH, false, CAMERA_FOLLOW_NONE);

  // Character 组件：拼写平台 = 大平台中央区域；注入放下落点解析与拼写事件
  d->owner = self->owner;
  CharacterInit(&d->character);
  d->character.app = d->app; // 注入音频宿主（拾取字母音效）
  d->character.dropResolver = SpellDropResolver;
  d->character.dropCtx = d;
  d->character.onSpellCorrect = SpellOnSpellCorrect;
  d->character.onSpellWrong = SpellOnSpellWrong;
  d->character.eventCtx = d;
  d->character.wordPlatform =
      (Rectangle){d->large_platform->spawnPosition.x +
                      d->large_platform->size.x * 0.5f - 90.0f,
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

  // 多挖空拼写：按难度决定挖空数量（简单 2 / 普通 3 / 困难 4，短词自动收敛）
  d->character.blankCount = 2 + d->difficulty;

  // 初始谜题 + 初始字母批 + 重置关卡状态
  CharacterSetupPuzzle(&d->character);
  d->timeLeft = SPELL_TIME_LIMIT;
  d->lastTickSecond = 0;
  d->fallingCount = 0;
  d->transitionRequested = false;
  // 字母批仅在「填对一个挖空后」掉落下一批（见 SpellOnSpellCorrect），
  // 无需定时器驱动
  SpellSpawnWave(d);

  // 隐式全局计时器：进入第一关开始计时（后续关卡保持累计）
  if (d->level == 1)
    SpeedrunStart((GameApp *)d->app);
}

static void SpellSceneUpdate(GameScene *self, float dt) {
  SpellSceneData *d = (SpellSceneData *)self->data;

  // HP 归零 → 失败
  if (d->cat->health <= 0.0f) {
    GameStackReplace(self->owner, FailSceneCreate(d->app));
    return;
  }

  // 倒计时：归零扣血并重置倒计时，留在本关继续（与平台/迷宫超时语义一致，
  // 避免「躺过」拼写关；2026-08 修正：原实现超时会直接进入下一关）
  d->timeLeft -= dt;
  if (d->timeLeft <= 0.0f) {
    d->cat->health -= TIME_PENALTY;
    if (d->cat->health < 0.0f)
      d->cat->health = 0.0f;
    d->timeLeft = SPELL_TIME_LIMIT;
    return;
  }
  // 剩余时间进入最后 COUNTDOWN_WARN_SECONDS 秒后，每跨一个整秒播放一次
  // tick 提示音（跨过 5/4/3/2/1 秒整各一声；归零/重置后自动重新武装）
  if (TimerCountdownWarn(&d->lastTickSecond, d->timeLeft,
                         COUNTDOWN_WARN_SECONDS))
    GameAppPlaySound(d->app, d->app->tickSound, d->app->tickSoundValid);

  // 字母批仅在「填对一个挖空后」掉落下一批（见 SpellOnSpellCorrect），
  // 无需按时间周期掉落

  // 下落字母：下落 + 落地并入 character
  SpellUpdateFalling(d, dt);

  // 平台跳跃物理（大平台）+ 水平/掉落钳制
  UpdatePlayer(d->cat, dt);
  d->cat->isOnTheGround = false;
  PlayerCollision(d->cat, d->large_platform);
  SpellClampPlayer(d);

  // 学习机制：复习横幅计时递减
  CharacterUpdateReview(&d->character, dt);

  // 字母拾取 / 放下 / 拼写判定
  CharacterUpdate(&d->character, d->cat);
  if (d->transitionRequested)
    return; // 拼写正确已请求切换场景，本帧不再继续

  // 动画帧（记录当前帧源矩形供绘制使用）
  d->rec =
      AnimationUpdate(&d->cat->animations[d->cat->playerAnimationState], dt);
}

// 关卡 HUD：左上角关卡号、顶部单词提示（挖空 + 词性 + 中文释义）、左下角
// 生命值条、右下角倒计时、右上角 ESC 提示、底部操作提示。背景为 RAYWHITE，
// 文字用黑色保证可读。
static void SpellDrawHud(SpellSceneData *d) {
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

  // 左下角：生命值条（全局 HUD，传入继承后的当前 HP）
  HudDrawHealthBar(d->app, d->cat->health, d->cat->maxHealth);
  // 右下角：剩余倒计时（全局 HUD）
  HudDrawTime(d->app, d->timeLeft);
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

static void SpellSceneDraw(GameScene *self) {
  SpellSceneData *d = (SpellSceneData *)self->data;

  // 锁定镜头（禁用相机）：Begin/End 为空操作，直接以世界坐标（== 屏幕坐标）绘制
  BeginSceneCamera(&d->camera);

  // 大平台（核心落脚点）
  DrawPlatform(d->large_platform);

  // 拼写平台：中央绿色高亮 + 提示
  DrawRectangleRec(d->character.wordPlatform, Fade(GREEN, 0.75f));
  DrawRectangleLinesEx(d->character.wordPlatform, 1.0f, GREEN);
  CharacterDrawSpellHint(&d->character, d->app);

  // 空中下落字母（尚未并入 character 的，含落地等待并入的）
  for (int i = 0; i < d->fallingCount; i++) {
    SpellFallingLetter *f = &d->falling[i];
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

  // 玩家
  DrawPlayer(d->cat, d->rec);

  // 头顶字母 + 虚线引导回拼写平台
  CharacterDrawHeld(&d->character, d->app, d->cat);

  EndSceneCamera(&d->camera);

  // 全局 HUD（固定逻辑屏幕坐标）
  SpellDrawHud(d);
}

// 场景创建后、onEnter 前被销毁（如过渡场景被中途打断时持有的未消费目标）
// —— 仅释放工厂 Create 阶段预分配的 cat / large_platform。贴图/词库在
// Enter 才加载，由 Exit 释放；不在此处理。必须无副作用（Exit 会写回
// app->playerHealth，不能用于未 Enter 的场景）。
static void SpellSceneDiscard(GameScene *self) {
  SpellSceneData *d = (SpellSceneData *)self->data;
  free(d->cat);
  free(d->large_platform);
  d->cat = NULL;
  d->large_platform = NULL;
}

static void SpellSceneExit(GameScene *self) {
  SpellSceneData *d = (SpellSceneData *)self->data;
  // 保存当前 HP 供下一关继承（失败/回菜单时由开始场景重置为 0）
  ((GameApp *)d->app)->playerHealth = d->cat->health;
  // 卸载本场景加载的资源（与 onEnter 一一配对）
  UnloadTexture(d->cat->idleTexture);
  UnloadTexture(d->cat->runTexture);
  UnloadTexture(d->cat->jumpTexture);
  UnloadTexture(d->cat->sleepTexture);
  UnloadTexture(d->cat->hitTexture);
  if (d->large_platform->platformTexture.id != 0)
    UnloadTexture(d->large_platform->platformTexture);
  CharacterFreeBank(&d->character);
  // 注意：cat 与 large_platform 由工厂分配，栈只释放 data，此处手动释放配对
  free(d->cat);
  free(d->large_platform);
}

GameScene *SpellSceneCreate(const GameApp *app, int difficulty, int level) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  if (scene == NULL) {
    return NULL;
  }
  SpellSceneData *data = (SpellSceneData *)calloc(1, sizeof(SpellSceneData));
  if (data == NULL) {
    free(scene);
    return NULL;
  }
  data->app = app;
  data->difficulty = difficulty;
  data->level = level;
  // 指针字段：cat / large_platform 由本工厂分配，Exit 释放（栈只释放 data）
  data->cat = (Player *)calloc(1, sizeof(Player));
  data->large_platform = (Platform *)calloc(1, sizeof(Platform));
  if (data->cat == NULL || data->large_platform == NULL) {
    free(data->cat);
    free(data->large_platform);
    free(data);
    free(scene);
    return NULL;
  }

  scene->name = "SpellScene";
  scene->data = data;
  scene->flags = GAME_SCENE_DRAW_WHEN_HIDDEN; // 隐藏时仍绘制
  scene->pauseable = true;                    // 可暂停
  scene->onEnter = SpellSceneEnter;
  scene->onDraw = SpellSceneDraw;
  scene->onUpdate = SpellSceneUpdate;
  scene->onExit = SpellSceneExit;
  scene->onDiscard = SpellSceneDiscard; // 未 Enter 即被销毁时释放子对象

  return scene;
}
