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

#endif // TIMER_H