#include "game.h"
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

// 向词库追加一个词条（自动扩容；realloc 失败时返回 false 且不改变原数组）。
static bool WordsBankAppend(WordsBank *bank, const char *word,
                            const char *meaning) {
  if (!bank || !word || !meaning)
    return false;
  if (word[0] == '\0' || meaning[0] == '\0')
    return false;
  if (bank->count >= bank->capacity) {
    int newCap =
        (bank->capacity == 0) ? WORDS_INIT_CAPACITY : bank->capacity * 2;
    WordEntry *newEntries =
        (WordEntry *)realloc(bank->entries, sizeof(WordEntry) * (size_t)newCap);
    if (!newEntries)
      return false;
    bank->entries = newEntries;
    bank->capacity = newCap;
  }
  WordEntry *e = &bank->entries[bank->count];
  snprintf(e->word, sizeof(e->word), "%s", word);
  snprintf(e->meaning, sizeof(e->meaning), "%s", meaning);
  ExtractPos(meaning, e->pos, sizeof(e->pos));
  bank->count++;
  return true;
}

// 从内存文本解析词库（不再依赖文件系统，配合内嵌资源使用）。
// 文本按 "\n" 切行，每行 "word<TAB>meaning"，跳过无 tab 或字段为空的非法行。
// 成功返回 0；参数非法 / 空数据返回 -1。
int WordsBankLoadFromMemory(WordsBank *bank, const char *text, size_t size) {
  if (!bank || !text)
    return -1;
  bank->entries = NULL;
  bank->count = 0;
  bank->capacity = 0;
  if (size == 0)
    return -1;

  size_t start = 0;
  for (size_t i = 0; i <= size; i++) {
    if (i == size || text[i] == '\n') {
      size_t len = i - start;
      if (len > 0 && text[i - 1] == '\r')
        len--; // 去掉行尾 \r（兼容 CRLF 词库）
      if (len > 0) {
        char line[512];
        if (len >= sizeof(line))
          len = sizeof(line) - 1;
        memcpy(line, text + start, len);
        line[len] = '\0';

        char *tab = strchr(line, '\t');
        if (tab) {
          *tab = '\0';
          if (!WordsBankAppend(bank, line, tab + 1))
            break; // 扩容失败：保留已加载词条并中断（不丢数据）
        }
      }
      start = i + 1;
    }
  }
  return 0;
}

// 从内嵌资源加载词库（relPath 形如 "assets/words/CET4.txt"）。
int WordsBankLoadEmbedded(WordsBank *bank, const char *relPath) {
  size_t size = 0;
  const unsigned char *data = EmbeddedAssetGet(relPath, &size);
  if (!data)
    return -1;
  return WordsBankLoadFromMemory(bank, (const char *)data, size);
}

int WordsBankLoad(WordsBank *bank, const char *path) {
  if (!bank || !path)
    return -1;
  FILE *fp = fopen(path, "rb");
  if (!fp)
    return -1;
  fseek(fp, 0, SEEK_END);
  long sz = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  if (sz <= 0) {
    fclose(fp);
    return -1;
  }
  char *buf = (char *)malloc((size_t)sz);
  if (!buf) {
    fclose(fp);
    return -1;
  }
  size_t rd = fread(buf, 1, (size_t)sz, fp);
  fclose(fp);
  int rc = WordsBankLoadFromMemory(bank, buf, rd);
  free(buf);
  return rc;
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
