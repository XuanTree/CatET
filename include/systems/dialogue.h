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

#endif // DIALOGUE_H