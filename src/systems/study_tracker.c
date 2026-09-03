#include "game.h"
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// 错词本 + 间隔重复抽词实现（见 include/systems/study_tracker.h）。
// 关键设计：
//   - 数组以词库下标为索引（与 WordsBank.entries 对齐），O(1) 查/改；
//   - 抽词分三阶段：到期错词（按拼错次数加权）→ 未答对新词 → 纯随机；
//   - 所有数组可为 NULL（空词库/分配失败），对应路径安全降级。
// ─────────────────────────────────────────────────────────────────────────────

// 确保 answered/wrongCount/lastWrongLevel 与词库大小匹配且已分配。
// 返回 true 表示数组可用（长度 == bank->count）；false 表示空词库/词库无
// entries/分配失败（调用方走纯随机/空操作降级，数组保持 NULL 或旧值）。
// 重建条件：数组缺失（新游戏未 Init / 上次分配失败），或 allocatedCount
// 与 bank->count 不符（跨难度换了词库：CET4 7508 词 vs CET6 5651 词）。
// 同尺寸重绑（同词库新实例、内容与下标顺序一致）时保留数组与记录，
// 实现跨关按下标延续错词记录；尺寸不符即词库不同，旧记录不迁移（清零）。
static bool StudyEnsureArrays(StudyTracker *t, const WordsBank *bank) {
  if (!bank || !bank->entries || bank->count <= 0)
    return false; // 空词库不建数组；已有数组保持不动（后续抽词会提前返回）
  const int n = bank->count;
  if (t->answered && t->wrongCount && t->lastWrongLevel &&
      t->allocatedCount == n) {
    return true; // 已分配且长度匹配
  }
  free(t->answered);
  free(t->wrongCount);
  free(t->lastWrongLevel);
  t->answered = (bool *)calloc((size_t)n, sizeof(bool));
  t->wrongCount = (int *)calloc((size_t)n, sizeof(int));
  t->lastWrongLevel = (int *)calloc((size_t)n, sizeof(int));
  t->allocatedCount = 0;
  if (!t->answered || !t->wrongCount || !t->lastWrongLevel) {
    // 任一分配失败：整体降级为纯随机（释放已分配部分并置 NULL）
    free(t->answered);
    free(t->wrongCount);
    free(t->lastWrongLevel);
    t->answered = NULL;
    t->wrongCount = NULL;
    t->lastWrongLevel = NULL;
    return false;
  }
  t->allocatedCount = n;
  return true;
}

void StudyInit(StudyTracker *t, const WordsBank *bank) {
  if (!t)
    return;
  memset(t, 0, sizeof(*t));
  t->bank = bank;
  StudyEnsureArrays(t, bank); // 空词库/分配失败：数组为 NULL，抽词降级
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
  t->allocatedCount = 0;
  t->bank = NULL;
  t->lastWrong = NULL;
}

void StudyRebind(StudyTracker *t, const WordsBank *bank) {
  if (!t)
    return;
  t->bank = bank;
  // 尺寸与现有数组不符（跨难度换词库）或数组缺失时重建，防按下标越界写；
  // 同尺寸重绑（同词库新实例）保留数组与错词记录（见 StudyEnsureArrays）。
  StudyEnsureArrays(t, bank);
}

void StudyReset(StudyTracker *t) {
  if (!t)
    return;
  // 清零长度依据 allocatedCount（数组真实长度），不读取 bank：
  // 回开始菜单等路径可能在上一个词库已释放后才调用 Reset，解引用
  // bank（悬垂）是 UB。allocatedCount 为 0 表示无数组，memset 空操作。
  const size_t n = (size_t)t->allocatedCount;
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
