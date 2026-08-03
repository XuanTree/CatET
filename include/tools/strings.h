#ifndef STRINGS_H
#define STRINGS_H

#pragma once
#include <stdio.h>
#include <string.h>

typedef struct String {
  char *data;
  size_t length;
} String;

// 这个头文件我自己都不知道写了有什么用

// Convert C string to String
String toString(const char *s);
void freeString(const String *s);

// Delete Characters in String;
String *delStringRight(String *s, int count);
String *delStringLeft(String *s, int count);

// Get Information from string
size_t getLength(const String *s);
char *getString(const String *s);
void printString(const String *s);

#endif // STRINGS_H
