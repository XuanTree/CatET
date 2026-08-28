/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef CHARACTER_H
#define CHARACTER_H

#pragma once
#include "core/gameapp.h"
#include "entities/player.h"
#include "systems/words_loader.h"
#include <raylib.h>
#include <stdbool.h>

// ─────────────────────────────────────────────────────────────────────────────
// 可复用的「字母拾取 + 拼写检查」组件（实体层）：
//   封装词库谜题（抽词/挖空）、字母实体、Z 键拾取/放下、拼写平台判定与
//   拼写正确/错误事件，通过回调与场景解耦（落点解析、拼写事件），
//   供迷宫、平台跳跃等任意场景复用（原实现内联在 scene_maze.c）。
// ─────────────────────────────────────────────────────────────────────────────

// 场景私有字母数组上限（正确字母 1 + 干扰字母）
#define CHARACTER_MAX_LETTERS 8
// 单词挖空数量上限（单词长度 3~12，此值足够容纳全部挖空下标）
#define CHARACTER_MAX_BLANKS 16

// 字母实体：被挖掉的正确字母或干扰字母。
typedef struct CharLetter {
  char ch;          // 字母字符
  bool isCorrect;   // 是否为需要的正确字母
  bool isPickedUp;  // 是否被玩家顶在头上
  Vector2 position; // 字母位置（世界坐标，落在某平台上方）
} CharLetter;

// 放下落点解析器：场景注入，根据玩家当前位置计算字母放下的落点
// （例如找玩家脚下最近的平台/墙体顶面，空中按 Z 放下也不悬空）。
// 返回落点世界坐标。
typedef Vector2 (*CharacterDropResolver)(void *ctx, const Player *p);

// 拼写事件回调（拼写正确 / 拼写错误），ctx 为场景注入的上下文。
typedef void (*CharacterEvent)(void *ctx);

// 可复用的字母交互 + 拼写检查组件。
typedef struct Character {
  const GameApp *app; // 音频宿主引用（由场景注入，用于播放 pick_letter 音效；
                      // NULL 时静默跳过）
  // 词库与当前谜题
  WordsBank bank;                       // 词库（由本组件加载/释放）
  WordEntry entry;                      // 当前单词
  char revealed[64];                    // 挖空后的单词显示（'_' 表示空位）
  int blankCount;                       // 挖空数量（1..单词长度，默认 1）
  int blankIndex[CHARACTER_MAX_BLANKS]; // 被挖空的字母在单词中的下标（升序）
  char answerChar; // 第一个挖空的正确字母（兼容单挖空场景：迷宫/暴击直接使用）

  // 字母实体与持有状态
  CharLetter letters[CHARACTER_MAX_LETTERS];
  int letterCount;
  bool holdingLetter;  // 玩家是否顶着一个字母
  int heldLetterIndex; // 当前顶着的字母下标（-1 表示无）

  // 拼写平台（世界坐标矩形，顶面供玩家站立）
  Rectangle wordPlatform;

  // 交互参数
  int pickupKey;      // 拾取/放下键（默认 KEY_Z）
  float pickupRadius; // 字母拾取半径（默认 22）

  // 场景注入回调
  CharacterDropResolver
      dropResolver;              // 放下落点解析（可为 NULL，退化为原地放下）
  void *dropCtx;                 // 传给 dropResolver 的上下文
  CharacterEvent onSpellCorrect; // 拼写正确事件（如切换下一关）
  CharacterEvent onSpellWrong;   // 拼写错误事件（如扣血）
  void *eventCtx;                // 传给事件回调的上下文
} Character;

// 初始化组件（清零状态、复位持有、默认键/半径）。
void CharacterInit(Character *c);

// 加载词库；成功返回 0，文件打不开返回 -1（见 WordsBankLoad）。
int CharacterLoadBank(Character *c, const char *path);

// 从内嵌资源加载词库，relPath 形如 "assets/words/CET4.txt"（见
// tools/resource.h）。
int CharacterLoadBankEmbedded(Character *c, const char *relPath);

// 释放词库内存。
void CharacterFreeBank(Character *c);

// 从词库抽取长度合适（3~12）的单词并按 blankCount（默认 1，场景可设为 >1
// 实现多挖空拼写）挖空多个字母，生成 revealed / blankIndex[] / answerChar。
// 词库为空或抽不到合适长度时使用兜底单词 "cat"（保证场景仍可运行）。
// 返回指向 entry.word 的指针（可直接用于 HUD 显示）。
const char *CharacterSetupPuzzle(Character *c);

// 返回仍未填写的挖空数量（0 表示全部挖空已填满，可通关）。
int CharacterRemainingBlanks(const Character *c);

// 判断字符 ch 是否为当前仍待填写的某个挖空字母。
bool CharacterIsAnswerLetter(const Character *c, char ch);

// 生成 1 个正确字母 + distractorCount 个干扰字母并放置到候选落点。
//   spots / spotIsDeadEnd 由场景提供：spots 为字母候选落点坐标，
//   spotIsDeadEnd 标记对应落点所在房间是否为死胡同（可为 NULL）；
//   内部洗牌后分配，并把正确字母优先放入“非死胡同”落点以保证可达，
//   全部为死胡同时退回随机（防御）。落点不足时按实际可用数减少字母数。
void CharacterPlaceLetters(Character *c, const Vector2 *spots,
                           const bool *spotIsDeadEnd, int spotCount,
                           int distractorCount);

// 每帧调用：处理拾取/放下/拼写判定。
//   - 未持有：与附近字母碰撞则拾取（顶在头上）；
//   - 持有且不在拼写平台：放下到 dropResolver 解析的落点（不判定、不扣血）；
//   - 持有且在拼写平台：字符匹配 answerChar 触发 onSpellCorrect，
//     否则把字母放回原位并触发 onSpellWrong。
void CharacterUpdate(Character *c, Player *p);

// 绘制未被拾取的字母（圆形 + 字符）。
void CharacterDrawLetters(const Character *c, const GameApp *app);

// 绘制头顶持有的字母 + 引导回拼写平台的虚线。
void CharacterDrawHeld(const Character *c, const GameApp *app, const Player *p);

// 在拼写平台上方绘制 "SPELL HERE" 提示。
void CharacterDrawSpellHint(const Character *c, const GameApp *app);

#endif // CHARACTER_H
