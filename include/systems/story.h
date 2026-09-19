/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SYSTEMS_STORY_H
#define SYSTEMS_STORY_H

#pragma once
#include "core/gameapp.h"
#include <stdbool.h>

// ─────────────────────────────────────────────────────────────────────────────
// 剧情覆盖层（系统层）：
//   玩家从主菜单选择 Play 进入关卡后，在「出生点」显示本关剧情文本，
//   文本以打字机方式逐字输出（参考 scene_battle 敌怪对话系统，每字 0.04s），
//   全程无需按键、无法跳过；输出完毕后文本框保留在出生点原地，不随玩家移动，
//   进入下一场景（下一关）时随场景销毁，并由新场景显示新的剧情。
//
//   显示位置为「世界坐标」：文本框以出生点为锚点，固定在关卡世界中该处，
//   玩家离开后不会跟随（不是贴在玩家身上的浮动标签）。
//
//   是否显示由 GameApp.isBeatGameOnce 决定：玩家完整通关过一次后，
//   后续游戏不再显示剧情（见 systems/save_data 的持久化字段）。
// ─────────────────────────────────────────────────────────────────────────────

// 剧情文本缓冲上限（getStory 单句最长约 90 字符，256 足够并留余量）
#define STORY_TEXT_MAX 256

// 文本框单行最大像素宽（文本超出自动按词换行）
#define STORY_BOX_MAX_WIDTH 260

// 文本框字号（像素字体）
#define STORY_BOX_FONT_SIZE 14

// 打字机一次性最多渲染的行数（超出部分不再换行，防御极端长文本）
#define STORY_MAX_LINES 8

typedef struct StoryOverlay {
  char text[STORY_TEXT_MAX]; // 本关剧情全文
  int charShown;             // 已逐字输出的字符数（打字机进度）
  float typeTimer;           // 逐字输出累计计时（秒）
  bool active;               // 是否有剧情需要绘制（无剧情时为 false）
  bool finished;             // 是否已完整输出（输出完毕后保留原地）
} StoryOverlay;

// 开始一段剧情：复制文本并重置打字机状态。
// text 为 NULL 或空串（Boss 关 / 走完一周目后）时视为无剧情，active=false。
void StoryOverlayStart(StoryOverlay *story, const char *text);

// 每帧推进打字机输出（dt 为 0 时冻结，暂停即暂停）。
void StoryOverlayUpdate(StoryOverlay *story, float dt);

// 清空覆盖层（active=false，不再绘制）。
void StoryOverlayClear(StoryOverlay *story);

// 是否已完整输出（输出完毕后文本框仍保留原地）。
bool StoryOverlayIsFinished(const StoryOverlay *story);

// 当前文本框高度（像素，含内边距）；无剧情时返回 0。
// 供调用方在绘制前计算锚点（例如把文本框底边对齐到出生点上方）。
int StoryOverlayHeight(const GameApp *app, const StoryOverlay *story,
                       int maxWidth, int fontSize);

// 绘制剧情文本框：以世界坐标 (anchorX, anchorY) 为锚点，文本框水平居中
// 于 anchorX、底边对齐 anchorY（即显示在锚点“上方”）。框左缘会钳制在
// [minX, maxX - 框宽] 内，避免出生点靠边时文本框跑出关卡边界。
// 需在场景相机（BeginSceneCamera/EndSceneCamera）之间调用，使文本固定在
// 关卡世界中的出生点处，不随玩家移动。
void StoryOverlayDrawAnchored(const GameApp *app, const StoryOverlay *story,
                              float anchorX, float anchorY, int maxWidth,
                              int fontSize, float minX, float maxX);

#endif // SYSTEMS_STORY_H
