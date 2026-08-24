#ifndef WORDS_LOADER_H
#define WORDS_LOADER_H

#pragma once
#include <stddef.h>

// ─────────────────────────────────────────────────────────────────────────────
// 词库加载器（系统层）：从 assets/words/ 下的 CET4/CET6 词库文件加载单词，
// 供各玩法场景随机抽取单词作为关卡核心元素。词库为 UTF-8 文本，每行格式：
//   word<TAB>词性. 释义 [词性. 释义 ...]
// 例如 "access<TAB>v. 获取 n. 接近，入口"。
// ─────────────────────────────────────────────────────────────────────────────

// 词条：单词 + 完整释义 + 首个词性缩写。
typedef struct WordEntry {
  char word[64];     // 单词（tab 之前的部分，ASCII 小写）
  char meaning[256]; // 释义原文（tab 之后，UTF-8，含中文）
  char pos[16];      // 首个词性缩写（如 "v." "n." "adj."），供英文 UI 显示
} WordEntry;

// 词库：一次性加载到内存的动态数组。
typedef struct WordsBank {
  WordEntry *entries;
  int count;
  int capacity;
} WordsBank;

// 从词库文件加载所有词条。成功返回 0；文件打不开返回 -1（entries 保持为空）。
// realloc 失败时保留已加载部分并中断（降级），已加载数量反映在 count 中。
int WordsBankLoad(WordsBank *bank, const char *path);

// 释放词库内存。
void WordsBankFree(WordsBank *bank);

// 随机返回一个词条指针；词库为空返回 NULL。
const WordEntry *WordsBankPickRandom(const WordsBank *bank);

#endif // WORDS_LOADER_H
