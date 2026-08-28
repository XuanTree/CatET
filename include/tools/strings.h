/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STRINGS_H
#define STRINGS_H

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// ─────────────────── String：安全的字符串缓冲区 ─────────────────────────────
// 与旧版「非拥有视图」不同，String 始终拥有自己的内存：
//   - data 指向以 '\0' 结尾的可写缓冲区（分配失败时可能为 NULL，按空串处理）
//   - length 为字符数，不含终止符 '\0'
//   - capacity 为可写容量（不含 '\0'），capacity == 0 表示尚未分配字符空间
// 由 StringCreate* 创建的 String 必须调用 StringFree 释放，杜绝悬垂指针。
typedef struct String {
  char *data;
  size_t length;
  size_t capacity;
} String;

// ─── 生命周期 ───────────────────────────────────────────────────────────────
// 复制创建（s 为 NULL 视为空串）；返回的 String 拥有内存，必须 StringFree
String StringCreate(const char *s);
// 复制 s 的前 n 个字符并补 '\0'（自动截断到 s 实际长度）；须 StringFree
String StringCreateN(const char *s, size_t n);
// 创建空字符串；须 StringFree
String StringCreateEmpty(void);
// 释放 String 拥有的内存，并把 s 复位为空串（s 必须非 NULL）
void StringFree(String *s);

// ─── 查询 ───────────────────────────────────────────────────────────────────
size_t StringLength(const String *s); // 字符数（不含 '\0'）
bool StringIsEmpty(const String *s);  // 是否为空串
const char *
StringData(const String *s);    // 只读 C 字符串（合法 String 永不为 NULL）
char *StringDataMut(String *s); // 可变访问；只能在 [0, length) 内写，保留 '\0'
char StringAt(const String *s, size_t index); // 取字符，越界返回 '\0'

// ─── 修改 ───────────────────────────────────────────────────────────────────
bool StringAppend(String *s, const char *text);          // 追加 C 字符串
bool StringAppendChar(String *s, char c);                // 追加单字符
bool StringAppendString(String *s, const String *other); // 追加另一个 String
void StringClear(String *s);                     // 清空内容（保留已分配容量）
void StringDeleteLeft(String *s, size_t count);  // 原地删除左侧 count 个字符
void StringDeleteRight(String *s, size_t count); // 原地删除右侧 count 个字符

// ─── 比较 ───────────────────────────────────────────────────────────────────
int StringCompare(const String *a, const String *b); // 语义同 strcmp
bool StringEquals(const String *a, const String *b);
bool StringEqualsCStr(const String *a, const char *b);

// ─── 输出 ───────────────────────────────────────────────────────────────────
void StringPrint(const String *s);                 // 打印到 stdout（末尾换行）
void StringPrintTo(const String *s, FILE *stream); // 打印到指定流（末尾换行）

#endif // STRINGS_H
