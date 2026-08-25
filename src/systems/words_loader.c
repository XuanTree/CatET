#include "systems/words_loader.h"
#include "tools/genrandom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORDS_INIT_CAPACITY 512

// 从释义字符串中提取首个词性缩写（第一个空白字符之前的 token）。
// 词库格式如 "v. 获取 n. 接近，入口"，首个 token "v." 即词性。
static void ExtractPos(const char *meaning, char *out, size_t outSize) {
  out[0] = '\0';
  if (!meaning || outSize == 0)
    return;
  size_t len = 0;
  while (meaning[len] != '\0' && meaning[len] != ' ' && meaning[len] != '\t' &&
         meaning[len] != '\n' && meaning[len] != '\r') {
    len++;
  }
  if (len >= outSize)
    len = outSize - 1;
  memcpy(out, meaning, len);
  out[len] = '\0';
}

int WordsBankLoad(WordsBank *bank, const char *path) {
  if (!bank || !path)
    return -1;
  bank->entries = NULL;
  bank->count = 0;
  bank->capacity = 0;

  FILE *fp = fopen(path, "rb");
  if (!fp)
    return -1;

  char line[512];
  // 逐行读取并解析 "word<TAB>meaning"，跳过无 tab 分隔或字段为空的非法行。
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\r\n")] = '\0'; // 去掉行尾换行
    char *tab = strchr(line, '\t');
    if (!tab)
      continue;
    *tab = '\0';
    const char *word = line;
    const char *meaning = tab + 1;
    if (word[0] == '\0' || meaning[0] == '\0')
      continue;

    // 动态扩容：realloc 失败时保留已加载词条并中断（不丢数据）。
    if (bank->count >= bank->capacity) {
      int newCap =
          (bank->capacity == 0) ? WORDS_INIT_CAPACITY : bank->capacity * 2;
      WordEntry *newEntries = (WordEntry *)realloc(
          bank->entries, sizeof(WordEntry) * (size_t)newCap);
      if (!newEntries)
        break;
      bank->entries = newEntries;
      bank->capacity = newCap;
    }

    WordEntry *e = &bank->entries[bank->count];
    snprintf(e->word, sizeof(e->word), "%s", word);
    snprintf(e->meaning, sizeof(e->meaning), "%s", meaning);
    ExtractPos(meaning, e->pos, sizeof(e->pos));
    bank->count++;
  }
  fclose(fp);
  return 0;
}

void WordsBankFree(WordsBank *bank) {
  if (!bank)
    return;
  free(bank->entries);
  bank->entries = NULL;
  bank->count = 0;
  bank->capacity = 0;
}

const WordEntry *WordsBankPickRandom(const WordsBank *bank) {
  if (!bank || bank->count <= 0)
    return NULL;
  return &bank->entries[genRandomNum(bank->count)];
}
