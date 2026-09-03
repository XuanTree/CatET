/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STUDY_TRACKER_H
#define STUDY_TRACKER_H

#pragma once
#include "systems/words_loader.h"
#include <stdbool.h>

// ─────────────────────────────────────────────────────────────────────────────
// 错词本 + 间隔重复抽词（系统层，plans/CatET_optimization_plan.md 方向B）：
//   为“背单词游戏”的核心服务——记录本局已答对/拼错的词条，抽词时：
//     1) 优先复现「间隔到期」的错词（拼错后至少隔 STUDY_REVISIT_INTERVAL 关），
//        按拼错次数加权，让常错的词更早、更频繁出现；
//     2) 其次抽「本局未答对」的新词（保证覆盖面）；
//     3) 全部答对后回退纯随机。
//   同一场景连续抽词可用 excludeWord 排除当前词，避免立即重复。
//   绑定一个词库（不拥有；难度切换时由场景重建）。空词库时所有函数安全
//   退化为纯随机/空操作。
// ─────────────────────────────────────────────────────────────────────────────

// 错词复现间隔（关卡数）：拼错后至少间隔该关数才再次复现
#define STUDY_REVISIT_INTERVAL 5

// 阶段1 到期错词复现的关卡间隔阈值：currentLevel - lastWrongLevel >= 该值
// （与 STUDY_REVISIT_INTERVAL 同义，供可读性）
#define STUDY_DUE_LEVEL_GAP STUDY_REVISIT_INTERVAL

typedef struct StudyTracker {
  const WordsBank *bank; // 关联词库（不拥有；难度切换时重建）
  bool *answered;        // 本局已答对标记（长度 allocatedCount）
  int *wrongCount;       // 本局拼错次数（间隔重复加权）
  int *lastWrongLevel;   // 最近拼错时的关卡号
  int allocatedCount; // 三个数组分配时的词条数（分配/重建时记录），供检测
                      // 词库尺寸变化与安全 memset，不依赖可能已释放的 bank
  int correctTotal;      // 累计答对（跨场景，供结算/复习参考）
  int wrongTotal;        // 累计拼错
  int currentLevel;      // 当前关卡号（场景每关 Enter 更新）
  const WordEntry
      *lastWrong; // 最近拼错词条（复习反馈，指向 bank 内部，不拥有）
} StudyTracker;

// 创建并绑定词库：分配 answered/wrongCount/lastWrongLevel（长度 bank->count）。
// 词库为空或分配失败时相应数组为 NULL，抽词自动退化为纯随机。
// 注意：StudyTracker 应在 GameApp 层全局持有（跨关卡共享），每局只 Init 一次。
void StudyInit(StudyTracker *t, const WordsBank *bank);

// 释放动态数组（不释放 bank）。之后 t 不可再用于抽词/标记。
void StudyFree(StudyTracker *t);

// 重新绑定词库指针：词库每关重新加载，虽为不同实例但内容与下标顺序一致，
// 跨关复用时只需换 bank 指针即可按下标保留错词记录。词库尺寸与现有数组
// 不符（跨难度 CET4/CET6 词数不同）或数组缺失（分配失败等）时重建数组
// （旧记录不跨词库迁移，重建即清零）。空词库不动作。
void StudyRebind(StudyTracker *t, const WordsBank *bank);

// 清空本局全部记录（新游戏/回开始菜单时调用）：保留数组与 bank 绑定，
// 仅清零 answered/wrongCount/lastWrongLevel 与统计计数，避免重复分配。
// 清零长度依据 allocatedCount，不读取 bank（其可能已被场景释放）。
void StudyReset(StudyTracker *t);

// 间隔重复抽词（详见文件头注释）。excludeWord 可传 NULL；
// 返回指向 bank 内部词条的指针；词库为空返回 NULL。
const WordEntry *StudyPickWord(const StudyTracker *t, const char *excludeWord);

// 标记答对：置 answered、清零错词记录、累计答对数。
void StudyMarkCorrect(StudyTracker *t, const WordEntry *e);

// 标记拼错：wrongCount++、记录 lastWrongLevel=level、更新
// lastWrong、累计拼错数。
void StudyMarkWrong(StudyTracker *t, const WordEntry *e, int level);

#endif // STUDY_TRACKER_H
