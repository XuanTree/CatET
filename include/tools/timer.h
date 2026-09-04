/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef TIMER_H
#define TIMER_H

#pragma once
#include <raylib.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct Timer {
  float startTime;
  float elapsedTime;
  bool isTimerStart;
  bool isTimerPause;
} Timer;

void InitTimer(Timer *timer);
void UpdateTimer(Timer *timer);
void PauseTimer(Timer *timer);
void ResetTimer(Timer *timer);
float GetElapsedTime(Timer *timer);

// 倒计时剩余警告（最后 warnAt 秒内每秒提示一次）：remaining（>0）进入 warnAt
// 秒以内后，首次跨过 warnAt 整秒以及之后每跨过一个整秒（warnAt-1、…、1）各返回
// true 一次——即最后 warnAt 秒的每一秒都触发（如剩余 5、4、3、2、1 秒整时各播
// 一次 tick.ogg）。state 指向场景内跨帧保存的整秒刻度（剩余时间向上取整，初值
// 0=尚未同步）；倒计时归零复位、重置到更大值后自动重新武装，下一轮递减时再次
// 逐秒触发。基于“上帧刻度与本帧刻度”比较，不依赖固定帧率；暂停时场景不更新
// 倒计时，state 自然冻结。
bool TimerCountdownWarn(int *state, float remaining, float warnAt);

#endif // TIMER_H
