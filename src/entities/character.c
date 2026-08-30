#include "game.h"

// ─────────────────────────────────────────────────────────────────────────────
// 可复用的「字母拾取 + 拼写检查」组件实现：
//   从原迷宫场景（scene_maze.c）抽取的字母交互逻辑，包含词库谜题生成、
//   Z 键拾取/放下、拼写平台判定与拼写事件。场景相关的行为（落点解析、
//   拼写正确/错误处理）通过回调注入，本组件不依赖任何具体场景。
// ─────────────────────────────────────────────────────────────────────────────

#define CHARACTER_DEFAULT_PICKUP_RADIUS 22.0f
// 拼错复习横幅显示时长（秒，最后 1 秒淡出）
#define CHARACTER_REVIEW_TIME 3.0f

// 玩家矩形（世界坐标，绘制与碰撞统一）
static Rectangle CharacterPlayerRect(const Player *p) {
  return (Rectangle){p->position.x, p->position.y, p->size.x, p->size.y};
}

void CharacterInit(Character *c) {
  if (!c)
    return;
  memset(c, 0, sizeof(*c));
  c->heldLetterIndex = -1;
  c->pickupKey = KEY_Z;
  c->pickupRadius = CHARACTER_DEFAULT_PICKUP_RADIUS;
  c->blankCount = 1; // 默认单挖空；场景可设 >1 实现多挖空拼写
}

int CharacterLoadBank(Character *c, const char *path) {
  if (!c || !path)
    return -1;
  return WordsBankLoad(&c->bank, path);
}

int CharacterLoadBankEmbedded(Character *c, const char *relPath) {
  if (!c || !relPath)
    return -1;
  return WordsBankLoadEmbedded(&c->bank, relPath);
}

void CharacterFreeBank(Character *c) {
  if (!c)
    return;
  WordsBankFree(&c->bank);
}

const char *CharacterSetupPuzzle(Character *c) {
  if (!c)
    return NULL;

  // 抽取长度合适的单词（3~12）。若绑定了错词本（study），优先用间隔重复
  // 抽词（排除当前词避免立即重复）；抽到的词长度不合适则回退随机。
  const WordEntry *entry = NULL;
  if (c->study) {
    entry = StudyPickWord(c->study,
                          (c->entry.word[0] != '\0') ? c->entry.word : NULL);
    if (entry) {
      const size_t slen = strlen(entry->word);
      if (slen < 3 || slen > 12)
        entry = NULL;
    }
  }
  if (!entry) {
    for (int i = 0; i < 200; i++) {
      const WordEntry *cand = WordsBankPickRandom(&c->bank);
      if (!cand)
        break;
      size_t len = strlen(cand->word);
      if (len >= 3 && len <= 12) {
        entry = cand;
        break;
      }
    }
  }
  if (entry) {
    c->entry = *entry;
  } else {
    // 词库为空或没有合适长度：使用兜底单词，保证场景仍可运行
    snprintf(c->entry.word, sizeof(c->entry.word), "cat");
    snprintf(c->entry.meaning, sizeof(c->entry.meaning), "n. (fallback)");
    snprintf(c->entry.pos, sizeof(c->entry.pos), "n.");
  }

  // 按 blankCount 挖空多个字母（默认 1；场景可设 >1）：随机选互不相同的
  // 下标，升序存入 blankIndex，并据此生成 revealed（'_' 表示空位）。
  size_t len = strlen(c->entry.word);
  int target = c->blankCount;
  if (target < 1)
    target = 1;
  if (target > (int)len)
    target = (int)len;
  if (target > CHARACTER_MAX_BLANKS)
    target = CHARACTER_MAX_BLANKS;
  c->blankCount = target;

  bool used[CHARACTER_MAX_BLANKS] = {false};
  int filled = 0;
  int guard = 0;
  while (filled < c->blankCount && guard < 500) {
    guard++;
    int idx = genRandomNum((int)len);
    if (used[idx])
      continue;
    used[idx] = true;
    c->blankIndex[filled++] = idx;
  }
  // 随机未能集齐（防御）：顺序补足
  for (int i = 0; i < (int)len && filled < c->blankCount; i++) {
    if (!used[i])
      c->blankIndex[filled++] = i;
  }
  // 升序排序（revealed 从左到右挖空顺序自然）
  for (int a = 0; a < c->blankCount; a++)
    for (int b = a + 1; b < c->blankCount; b++)
      if (c->blankIndex[b] < c->blankIndex[a]) {
        int t = c->blankIndex[a];
        c->blankIndex[a] = c->blankIndex[b];
        c->blankIndex[b] = t;
      }

  // 用 String 逐字符构建挖空显示（每关开始调用一次，非热路径）
  String revealed = StringCreateEmpty();
  for (size_t i = 0; i < len; i++) {
    bool blanked = false;
    for (int k = 0; k < c->blankCount; k++)
      if (c->blankIndex[k] == (int)i) {
        blanked = true;
        break;
      }
    StringAppendChar(&revealed, blanked ? '_' : c->entry.word[i]);
  }
  snprintf(c->revealed, sizeof(c->revealed), "%s", StringData(&revealed));
  StringFree(&revealed);

  // 兼容保留：第一个挖空的字母（迷宫/暴击场景单挖空仍直接使用 answerChar）
  c->answerChar = c->entry.word[c->blankIndex[0]];
  return c->entry.word;
}

// 返回仍未填写的挖空数量（0 表示全部挖空已填满）
int CharacterRemainingBlanks(const Character *c) {
  if (!c)
    return 0;
  int n = 0;
  for (int i = 0; i < c->blankCount; i++) {
    int idx = c->blankIndex[i];
    if (idx >= 0 && idx < 64 && c->revealed[idx] == '_')
      n++;
  }
  return n;
}

// 判断字符 ch 是否为当前仍待填写的某个挖空字母
bool CharacterIsAnswerLetter(const Character *c, char ch) {
  if (!c)
    return false;
  for (int i = 0; i < c->blankCount; i++) {
    int idx = c->blankIndex[i];
    if (idx < 0 || idx >= 64)
      continue;
    if (c->revealed[idx] != '_')
      continue; // 已填的挖空不再是候选
    if (c->entry.word[idx] == ch)
      return true;
  }
  return false;
}

void CharacterPlaceLetters(Character *c, const Vector2 *spots,
                           const bool *spotIsDeadEnd, int spotCount,
                           int distractorCount) {
  if (!c || !spots || spotCount <= 0)
    return;
  if (distractorCount < 0)
    distractorCount = 0;
  if (distractorCount > CHARACTER_MAX_LETTERS - 1)
    distractorCount = CHARACTER_MAX_LETTERS - 1;

  // 干扰字母（不同于正确字母的小写字母）
  char distractors[CHARACTER_MAX_LETTERS - 1];
  for (int i = 0; i < distractorCount; i++) {
    char ch;
    do {
      ch = (char)('a' + genRandomNum(26));
    } while (ch == c->answerChar);
    distractors[i] = ch;
  }

  // 洗牌候选落点（同步打乱死胡同标记），保证每局字母位置随机
  Vector2 shuffled[CHARACTER_MAX_LETTERS];
  bool shuffledDeadEnd[CHARACTER_MAX_LETTERS] = {false};
  int n =
      (spotCount < CHARACTER_MAX_LETTERS) ? spotCount : CHARACTER_MAX_LETTERS;
  for (int i = 0; i < n; i++) {
    shuffled[i] = spots[i];
    if (spotIsDeadEnd)
      shuffledDeadEnd[i] = spotIsDeadEnd[i];
  }
  for (int i = n - 1; i > 0; i--) {
    int j = genRandomNum(i + 1);
    Vector2 t = shuffled[i];
    shuffled[i] = shuffled[j];
    shuffled[j] = t;
    bool tb = shuffledDeadEnd[i];
    shuffledDeadEnd[i] = shuffledDeadEnd[j];
    shuffledDeadEnd[j] = tb;
  }

  c->letterCount = 1 + distractorCount;
  if (c->letterCount > n)
    c->letterCount = n; // 落点不足时减少字母数（防御）

  // 正确字母优先放在“非死胡同”落点（可通过房间），保证
  // 出生点→正确字母→拼写平台之间无死路；全部为死胡同时退回随机（防御）。
  int correctIdx = -1;
  for (int i = 0; i < c->letterCount; i++) {
    if (!shuffledDeadEnd[i]) {
      correctIdx = i;
      break;
    }
  }
  if (correctIdx < 0)
    correctIdx = genRandomNum(c->letterCount);

  int distIdx = 0;
  for (int i = 0; i < c->letterCount; i++) {
    c->letters[i].isCorrect = (i == correctIdx);
    c->letters[i].ch =
        c->letters[i].isCorrect ? c->answerChar : distractors[distIdx++];
    c->letters[i].isPickedUp = false;
    c->letters[i].position = shuffled[i];
  }
  c->holdingLetter = false;
  c->heldLetterIndex = -1;
}

void CharacterUpdate(Character *c, Player *p) {
  if (!c || !p)
    return;
  if (!IsKeyPressed(c->pickupKey))
    return;

  if (!c->holdingLetter) {
    // 未持有：靠近字母则拾取（顶在头上）
    for (int i = 0; i < c->letterCount; i++) {
      if (c->letters[i].isPickedUp)
        continue;
      if (CheckCollisionCircleRec(c->letters[i].position, c->pickupRadius,
                                  CharacterPlayerRect(p))) {
        c->letters[i].isPickedUp = true;
        c->holdingLetter = true;
        c->heldLetterIndex = i;
        // 拾取音效（pick_letter.ogg）：捡起字母时播放一次
        GameAppPlaySound(c->app, c->app->pickLetterSound,
                         c->app->pickLetterSoundValid);
        break;
      }
    }
    return;
  }

  if (c->heldLetterIndex < 0 || c->heldLetterIndex >= c->letterCount)
    return;
  CharLetter *held = &c->letters[c->heldLetterIndex];

  // 放下：玩家中心落在拼写平台及其上方空间内 → 拼写判定；
  // 否则可在任意位置放下字母（可随时放弃/更换，避免误扣血）。
  Rectangle dropArea = {c->wordPlatform.x, c->wordPlatform.y - 48,
                        c->wordPlatform.width, c->wordPlatform.height + 48};
  Vector2 center = {p->position.x + p->size.x * 0.5f,
                    p->position.y + p->size.y * 0.5f};
  if (!CheckCollisionPointRec(center, dropArea)) {
    // 任意位置放下：字母落到解析器给出的平台顶面（空中按 Z 也不会悬空）
    if (c->dropResolver)
      held->position = c->dropResolver(c->dropCtx, p);
    held->isPickedUp = false;
    c->holdingLetter = false;
    c->heldLetterIndex = -1;
    return;
  }

  if (CharacterIsAnswerLetter(c, held->ch)) {
    // 拼写正确：填掉一个匹配的挖空并更新 revealed（多挖空逐空填满），
    // 复位持有状态，交由场景事件决定「继续下一批」或「通关」。
    for (int i = 0; i < c->blankCount; i++) {
      int idx = c->blankIndex[i];
      if (idx < 0 || idx >= 64 || c->revealed[idx] != '_')
        continue; // 该挖空已填
      if (c->entry.word[idx] == held->ch) {
        c->revealed[idx] = held->ch; // 填上字母，HUD 显示逐步补全
        break;
      }
    }
    c->holdingLetter = false;
    c->heldLetterIndex = -1;
    // 学习机制：标记答对（清除错词记录）并关闭复习横幅
    if (c->study)
      StudyMarkCorrect(c->study, &c->entry);
    c->reviewTimer = 0.0f;
    if (c->onSpellCorrect)
      c->onSpellCorrect(c->eventCtx);
  } else {
    // 拼写错误：字母放回原位（重新寻找），触发场景事件（如扣血）
    held->isPickedUp = false;
    c->holdingLetter = false;
    c->heldLetterIndex = -1;
    // 学习机制：标记拼错并记录复习词条。须在 onSpellWrong 之前记录，
    // 因为场景回调可能重置谜题覆盖 entry。
    if (c->study)
      StudyMarkWrong(c->study, &c->entry, c->study->currentLevel);
    c->reviewEntry = c->entry;
    c->reviewTimer = CHARACTER_REVIEW_TIME;
    if (c->onSpellWrong)
      c->onSpellWrong(c->eventCtx);
  }
}

// 绘制单个字母（统一颜色，不区分正确/错误，让玩家凭单词提示自行判断）。
static void CharacterDrawOne(const GameApp *app, const CharLetter *l,
                             float radius) {
  DrawCircleV(l->position, radius, Fade(SKYBLUE, 0.85f));
  char txt[2] = {l->ch, '\0'};
  const int fs = 24;
  int w = GameAppMeasureText(app, txt, fs);
  GameAppDrawText(app, txt, (int)(l->position.x - w * 0.5f),
                  (int)(l->position.y - fs * 0.5f), fs, DARKBLUE);
}

void CharacterDrawLetters(const Character *c, const GameApp *app) {
  if (!c || !app)
    return;
  for (int i = 0; i < c->letterCount; i++) {
    if (!c->letters[i].isPickedUp)
      CharacterDrawOne(app, &c->letters[i], c->pickupRadius);
  }
}

void CharacterDrawHeld(const Character *c, const GameApp *app,
                       const Player *p) {
  if (!c || !app || !p || !c->holdingLetter)
    return;
  if (c->heldLetterIndex < 0 || c->heldLetterIndex >= c->letterCount)
    return;
  // 头顶字母 + 虚线引导回拼写平台
  CharLetter top = c->letters[c->heldLetterIndex];
  top.position =
      (Vector2){p->position.x + p->size.x * 0.5f, p->position.y - 12.0f};
  CharacterDrawOne(app, &top, c->pickupRadius);
  Vector2 to = {c->wordPlatform.x + c->wordPlatform.width * 0.5f,
                c->wordPlatform.y};
  DrawLineEx(top.position, to, 2.0f, Fade(BLUE, 0.7f));
}

void CharacterDrawSpellHint(const Character *c, const GameApp *app) {
  if (!c || !app)
    return;
  // 拼写平台上方提示（置于平台之上、上层楼板之下的走廊空腔中）
  const char *dropHint = "SPELL HERE";
  int dhSize = 16;
  int dhW = GameAppMeasureText(app, dropHint, dhSize);
  GameAppDrawText(
      app, dropHint,
      (int)(c->wordPlatform.x + c->wordPlatform.width * 0.5f - dhW * 0.5f),
      (int)(c->wordPlatform.y - dhSize - 8), dhSize, GREEN);
}

// 每帧递减拼错复习横幅计时（暂停时 dt=0 自然冻结）
void CharacterUpdateReview(Character *c, float dt) {
  if (!c || c->reviewTimer <= 0.0f)
    return;
  c->reviewTimer -= dt;
  if (c->reviewTimer < 0.0f)
    c->reviewTimer = 0.0f;
}

// 绘制拼错复习横幅：居中偏上，白底黑字 + 绿色边框，随剩余时间淡出。
// 内容：正确拼写 + 词性 + 中文释义（学习机制：拼错即复习）。
void CharacterDrawReviewBanner(const Character *c, const GameApp *app) {
  if (!c || !app || c->reviewTimer <= 0.0f)
    return;
  const int screenW = app->logicWidth;
  char line[384];
  snprintf(line, sizeof(line), "正确：%s  (%s %s)", c->reviewEntry.word,
           c->reviewEntry.pos, c->reviewEntry.meaning);
  const int fs = 18;
  const int tw = GameAppMeasureText(app, line, fs);
  const float pad = 14.0f;
  const float bw = (float)tw + pad * 2.0f;
  const float bh = (float)fs + 20.0f;
  const float bx = ((float)screenW - bw) * 0.5f;
  const float by = 86.0f;
  const float alpha = (c->reviewTimer < 1.0f) ? c->reviewTimer : 1.0f;
  DrawRectangle((int)bx, (int)by, (int)bw, (int)bh, Fade(WHITE, alpha * 0.92f));
  DrawRectangleLines((int)bx, (int)by, (int)bw, (int)bh, Fade(GREEN, alpha));
  GameAppDrawText(app, line, (int)(bx + pad), (int)(by + 10.0f), fs,
                  Fade(BLACK, alpha));
}
