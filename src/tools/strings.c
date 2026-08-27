#include "game.h"

// 返回安全的只读 C 字符串：data 为 NULL（分配失败）时视为空串
static const char *StringCStr(const String *s) {
  return (s && s->data) ? s->data : "";
}

// 向 s 追加 n 个字符；失败返回 false 且 s 保持原状。
// 注意：realloc 失败会返回 NULL，必须先暂存旧指针再回滚，避免原缓冲泄漏
// （对应安全审计 M1）。
static bool StringAppendN(String *s, const char *text, size_t n) {
  if (!s || !text)
    return false;
  if (n == 0)
    return true;
  if (n > SIZE_MAX - s->length) // 防 length 溢出
    return false;

  const size_t need = s->length + n;
  if (need > s->capacity) {
    size_t newCap = (s->capacity > 0) ? s->capacity : 1;
    while (newCap < need) {
      if (newCap > SIZE_MAX / 2) { // 防容量溢出
        newCap = need;
        break;
      }
      newCap *= 2;
    }
    char *newBuf = (char *)realloc(s->data, newCap + 1);
    if (!newBuf)
      return false;
    s->data = newBuf;
    s->capacity = newCap;
  }
  memcpy(s->data + s->length, text, n);
  s->length = need;
  s->data[need] = '\0';
  return true;
}

// ─── 生命周期 ───────────────────────────────────────────────────────────────

String StringCreate(const char *s) {
  if (!s)
    return StringCreateEmpty();
  return StringCreateN(s, strlen(s));
}

String StringCreateN(const char *s, size_t n) {
  String str = {0};
  if (!s)
    return str; // 输入非法，返回空串
  const size_t len = strlen(s);
  if (n > len)
    n = len; // 截断，避免越界读
  char *buf = (char *)malloc(n + 1);
  if (!buf)
    return str; // 分配失败，返回空串（data == NULL）
  if (n > 0)
    memcpy(buf, s, n);
  buf[n] = '\0';
  str.data = buf;
  str.length = n;
  str.capacity = n;
  return str;
}

String StringCreateEmpty(void) {
  String str = {0};
  str.data = (char *)malloc(1);
  if (str.data) {
    str.data[0] = '\0';
    str.length = 0;
    str.capacity = 0; // 仅保留终止符空间，追加首个字符时才扩容
  }
  return str;
}

void StringFree(String *s) {
  if (!s)
    return;
  free(s->data);
  // 复位为空串：即使误二次调用也不会双重释放
  s->data = NULL;
  s->length = 0;
  s->capacity = 0;
}

// ─── 查询 ───────────────────────────────────────────────────────────────────

size_t StringLength(const String *s) { return s ? s->length : 0; }

bool StringIsEmpty(const String *s) { return StringLength(s) == 0; }

const char *StringData(const String *s) { return StringCStr(s); }

char *StringDataMut(String *s) { return s ? s->data : NULL; }

char StringAt(const String *s, size_t index) {
  if (!s || index >= s->length)
    return '\0'; // 越界返回 '\0'
  return s->data[index];
}

// ─── 修改 ───────────────────────────────────────────────────────────────────

bool StringAppend(String *s, const char *text) {
  if (!text)
    return false;
  return StringAppendN(s, text, strlen(text));
}

bool StringAppendChar(String *s, char c) { return StringAppendN(s, &c, 1); }

bool StringAppendString(String *s, const String *other) {
  if (!other)
    return false;
  return StringAppendN(s, StringCStr(other), other->length);
}

void StringClear(String *s) {
  if (!s)
    return;
  if (s->data)
    s->data[0] = '\0';
  s->length = 0;
}

void StringDeleteLeft(String *s, size_t count) {
  if (!s)
    return;
  if (count >= s->length) {
    StringClear(s);
    return;
  }
  // 注意：用 memmove 将尾部前移，数据仍在原缓冲内，之后可安全 StringFree
  memmove(s->data, s->data + count, s->length - count);
  s->length -= count;
  s->data[s->length] = '\0';
}

void StringDeleteRight(String *s, size_t count) {
  if (!s)
    return;
  if (count >= s->length) {
    StringClear(s);
    return;
  }
  s->length -= count;
  s->data[s->length] = '\0';
}

// ─── 比较 ───────────────────────────────────────────────────────────────────

int StringCompare(const String *a, const String *b) {
  if (!a || !b)
    return a ? 1 : (b ? -1 : 0);
  const size_t minLen = (a->length < b->length) ? a->length : b->length;
  if (minLen > 0) {
    const int cmp = memcmp(StringCStr(a), StringCStr(b), minLen);
    if (cmp != 0)
      return cmp;
  }
  if (a->length < b->length)
    return -1;
  if (a->length > b->length)
    return 1;
  return 0;
}

bool StringEquals(const String *a, const String *b) {
  if (!a || !b)
    return false;
  if (a->length != b->length)
    return false;
  if (a->length == 0)
    return true; // 两个空串相等，避免对 NULL 调用 memcmp
  return memcmp(StringCStr(a), StringCStr(b), a->length) == 0;
}

bool StringEqualsCStr(const String *a, const char *b) {
  if (!a || !b)
    return false;
  return strcmp(StringCStr(a), b) == 0;
}

// ─── 输出 ───────────────────────────────────────────────────────────────────

void StringPrint(const String *s) { StringPrintTo(s, stdout); }

void StringPrintTo(const String *s, FILE *stream) {
  if (!s || !stream)
    return;
  // %.*s 的精度是 int，超长时截断以规避负数/溢出
  const int len = (s->length > (size_t)INT_MAX) ? INT_MAX : (int)s->length;
  fprintf(stream, "%.*s\n", len, StringCStr(s));
}
