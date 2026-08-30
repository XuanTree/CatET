#include "game.h"
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// 错词本 + 间隔重复抽词实现（见 include/systems/study_tracker.h）。
// 关键设计：
//   - 数组以词库下标为索引（与 WordsBank.entries 对齐），O(1) 查/改；
//   - 抽词分三阶段：到期错词（按拼错次数加权）→ 未答对新词 → 纯随机；
//   - 所有数组可为 NULL（空词库/分配失败），对应路径安全降级。
// ─────────────────────────────────────────────────────────────────────────────

void StudyInit(StudyTracker *t, const WordsBank *bank) {
  if (!t)
    return;
  memset(t, 0, sizeof(*t));
  t->bank = bank;
  if (bank && bank->count > 0 && bank->entries) {
    const size_t n = (size_t)bank->count;
    t->answered = (bool *)calloc(n, sizeof(bool));
    t->wrongCount = (int *)calloc(n, sizeof(int));
    t->lastWrongLevel = (int *)calloc(n, sizeof(int));
    // 任一分配失败：整体降级为纯随机（释放已分配部分并置 NULL）
    if (!t->answered || !t->wrongCount || !t->lastWrongLevel) {
      free(t->answered);
      free(t->wrongCount);
      free(t->lastWrongLevel);
      t->answered = NULL;
      t->wrongCount = NULL;
      t->lastWrongLevel = NULL;
    }
  }
}

void StudyFree(StudyTracker *t) {
  if (!t)
    return;
  free(t->answered);
  free(t->wrongCount);
  free(t->lastWrongLevel);
  t->answered = NULL;
  t->wrongCount = NULL;
  t->lastWrongLevel = NULL;
  t->bank = NULL;
  t->lastWrong = NULL;
}

void StudyRebind(StudyTracker *t, const WordsBank *bank) {
  if (!t)
    return;
  t->bank = bank;
  // 防御：词库非空但数组缺失（新游戏未 Init / 上次分配失败）→ 补建数组。
  // 同难度词库固定，正常路径数组已在 StudyInit 分配，这里不会重建。
  if (bank && bank->count > 0 && bank->entries &&
      (!t->answered || !t->wrongCount || !t->lastWrongLevel)) {
    const size_t sz = (size_t)bank->count;
    free(t->answered);
    free(t->wrongCount);
    free(t->lastWrongLevel);
    t->answered = (bool *)calloc(sz, sizeof(bool));
    t->wrongCount = (int *)calloc(sz, sizeof(int));
    t->lastWrongLevel = (int *)calloc(sz, sizeof(int));
    if (!t->answered || !t->wrongCount || !t->lastWrongLevel) {
      free(t->answered);
      free(t->wrongCount);
      free(t->lastWrongLevel);
      t->answered = NULL;
      t->wrongCount = NULL;
      t->lastWrongLevel = NULL;
    }
  }
}

void StudyReset(StudyTracker *t) {
  if (!t)
    return;
  const size_t n = (t->bank && t->bank->entries) ? (size_t)t->bank->count : 0;
  if (t->answered)
    memset(t->answered, 0, sizeof(bool) * n);
  if (t->wrongCount)
    memset(t->wrongCount, 0, sizeof(int) * n);
  if (t->lastWrongLevel)
    memset(t->lastWrongLevel, 0, sizeof(int) * n);
  t->correctTotal = 0;
  t->wrongTotal = 0;
  t->currentLevel = 0;
  t->lastWrong = NULL;
}

// 词条指针在词库中的下标；不在该词库内返回 -1（防御）。
static int StudyIndex(const StudyTracker *t, const WordEntry *e) {
  if (!t || !t->bank || !t->bank->entries || !e)
    return -1;
  const ptrdiff_t idx = e - t->bank->entries;
  if (idx < 0 || idx >= t->bank->count)
    return -1;
  return (int)idx;
}

const WordEntry *StudyPickWord(const StudyTracker *t, const char *excludeWord) {
  if (!t || !t->bank || t->bank->count <= 0 || !t->bank->entries)
    return NULL;
  const int n = t->bank->count;

  // ── 阶段1：到期错词，按拼错次数加权随机 ──────────────────────────────
  if (t->wrongCount && t->lastWrongLevel) {
    int *due = (int *)malloc(sizeof(int) * (size_t)n);
    if (due) {
      int dueCount = 0;
      int totalWeight = 0;
      for (int i = 0; i < n; i++) {
        if (t->wrongCount[i] > 0 &&
            t->currentLevel - t->lastWrongLevel[i] >= STUDY_DUE_LEVEL_GAP) {
          due[dueCount++] = i;
          totalWeight += t->wrongCount[i];
        }
      }
      if (dueCount > 0 && totalWeight > 0) {
        int pick = genRandomNum(totalWeight); // [0, totalWeight)
        int chosen = due[dueCount - 1];
        for (int i = 0; i < dueCount; i++) {
          pick -= t->wrongCount[due[i]];
          if (pick < 0) {
            chosen = due[i];
            break;
          }
        }
        const WordEntry *res = &t->bank->entries[chosen];
        free(due);
        if (!excludeWord || strcmp(res->word, excludeWord) != 0)
          return res;
        // 与排除词相同：落入阶段2/3 兜底
      } else {
        free(due);
      }
    }
  }

  // ── 阶段2：本局未答对的新词 ────────────────────────────────────────────
  if (t->answered) {
    int unseen = 0;
    for (int i = 0; i < n; i++)
      if (!t->answered[i])
        unseen++;
    if (unseen > 0) {
      int target = genRandomNum(unseen);
      int chosen = -1;
      for (int i = 0; i < n; i++) {
        if (!t->answered[i]) {
          if (target == 0) {
            chosen = i;
            break;
          }
          target--;
        }
      }
      if (chosen >= 0) {
        const WordEntry *res = &t->bank->entries[chosen];
        if (!excludeWord || strcmp(res->word, excludeWord) != 0)
          return res;
      }
    }
  }

  // ── 阶段3：纯随机兜底（词库全部答对 / 数组缺失）──────────────────────
  return WordsBankPickRandom(t->bank);
}

void StudyMarkCorrect(StudyTracker *t, const WordEntry *e) {
  if (!t || !e)
    return;
  t->correctTotal++;
  const int idx = StudyIndex(t, e);
  if (idx < 0)
    return;
  if (t->answered)
    t->answered[idx] = true;
  if (t->wrongCount)
    t->wrongCount[idx] = 0;
  if (t->lastWrongLevel)
    t->lastWrongLevel[idx] = 0;
  if (t->lastWrong == e)
    t->lastWrong = NULL;
}

void StudyMarkWrong(StudyTracker *t, const WordEntry *e, int level) {
  if (!t || !e)
    return;
  t->wrongTotal++;
  const int idx = StudyIndex(t, e);
  if (idx < 0)
    return;
  if (t->wrongCount)
    t->wrongCount[idx]++;
  if (t->lastWrongLevel)
    t->lastWrongLevel[idx] = level;
  t->lastWrong = e;
}
