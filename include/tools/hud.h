/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef TOOLS_HUD_H
#define TOOLS_HUD_H

#pragma once
#include "core/gameapp.h"
#include <raylib.h>

// ─────────────────────────────────────────────────────────────────────────────
// 全局关卡 HUD 绘制工具（从 scene_test.c 抽离的可复用组件）：
//   提供各场景共用的 HUD 元素：左下角生命值条、左上角关卡号、右下角时间、
//   右上角 ESC 暂停提示。固定绘制于逻辑屏幕坐标（在场景相机之外调用），
//   供平台跳跃、迷宫解密等任意场景复用，避免每个场景各自实现一遍。
//
//   生命值继承：玩家实体进入场景时已从 app->playerHealth 恢复上一关剩余
//   HP（见各场景 onEnter），HUD 只负责把当前玩家的 health / maxHealth
//   传入 HudDrawHealthBar 绘制，无需关心继承细节。
// ─────────────────────────────────────────────────────────────────────────────

// 左下角生命值条：HP 标签 + 数值文本（cur/max）+ 进度条，
// 颜色随剩余血量变化（>50% 绿，25%~50% 橙，<=25% 红）。
// health / maxHealth 为当前玩家生命值与上限（进入场景时已继承
// app->playerHealth）。
void HudDrawHealthBar(const GameApp *app, float health, float maxHealth);

// 左上角：当前关卡编号（"Level : %d"）。
void HudDrawLevel(const GameApp *app, int level);

// 右下角：游戏时间 / 剩余时间（"Time mm:ss"，右对齐）。
void HudDrawTime(const GameApp *app, float timeSeconds);

// 右上角：方框内 ESC 提示（提示玩家按 ESC 暂停）。
void HudDrawEscHint(const GameApp *app);

// 单词选项行自适应布局（scene_battle / scene_infinite 的三选一共用）：
//   每个选项框宽度随对应单词长度浮动（词宽 + padX×2，且不低于 minBoxW），
//   全部框与 gap 一起在 availW 宽度内居中；若总宽放不下，逐档缩小字号
//   （每次 -2，从 baseFontSize 到 minFontSize）重新测量，极端长词仍超宽时
//   改为从左侧排布（防御，保证三个框互不重叠、都可读）。
//   words[i] 为候选单词文本（内部按 "" 防空）；count 为选项数。
//   返回实际使用的字号；outRects[i] 填入各框矩形（含统一的 y/height）。
int HudLayoutWordRow(const GameApp *app, const char *const *words, int count,
                     float availW, int gap, int padX, int baseFontSize,
                     int minFontSize, float minBoxW, float boxH, float y,
                     Rectangle *outRects);

#endif // TOOLS_HUD_H
