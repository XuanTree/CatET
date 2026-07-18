#include "core/strings.h"

String toString(const char *s) {
  return (String){
      .data = (char *)s,
      .length = strlen(s),
  };
}

String *delStringLeft(String *s, int count) {
  if (!s || count <= 0)
    return s;
  if ((size_t)count >= s->length) {
    s->length = 0;
    return s;
  }
  s->data += count;
  s->length -= count;
  return s;
}

void freeString(const String *s) {
  // String is a non-owning view; no dynamic memory to free.
  (void)s;
}

String *delStringRight(String *s, int count) {
  if (!s || count <= 0)
    return s;
  if ((size_t)count >= s->length) {
    s->length = 0;
    return s;
  }
  s->length -= count;
  return s;
}

size_t getLength(String *s) { return s->length; }
char *getString(String *s) { return s->data; }
void printString(String *s) { printf("%.*s\n", (int)s->length, s->data); }