/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef DIALOGUE_H
#define DIALOGUE_H

#pragma once
#include "game.h"
#include "tools/genrandom.h"

/* 用于给敌怪随机提供对话的函数
  每次调用会返回一个字符串，代表一个对话

  敌怪对话时，会随机选择一个对话，然后逐个字符输出
  对话框采用白底黑字的形式,并在对话框最底部固定显示
  "press x to skip"

  不按x键，对话会自动逐个字符输出, 直到输出完整个对话
  对话显示完2s后,若玩家没有按x,也自动开始攻击[发射弹幕]
*/

const char *getDialogue();

/* 用于生成剧情的函数。
 * 给这个破游戏写剧情，我真是疯子
 * 剧情文字会自动在进入新的关卡时自动显示，逐字输出，无法跳过，与玩家的操作无关
 * （显示由 systems/story 的 StoryOverlay 负责）。
 * 返回空串表示该关卡没有剧情（Boss 关 / 最终关 / 困难难度未覆盖的关卡），
 * 剧情系统据此不显示文本框；玩家完整通关一次后由 isBeatGameOnce 跳过显示。
 */

const char *getStory(int difficulty, int level);

/*
 * 用于生成启动游戏时的随机文本
 * 每次调用会返回一个字符串
 */

const char *getStartText();

#endif // DIALOGUE_H
