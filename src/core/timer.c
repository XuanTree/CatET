#include "core/timer.h"

void InitTimer(Timer *timer) {
  timer->isTimerStart = false;
  timer->isTimerPause = false;
  timer->startTime = GetTime();
}

void UpdateTimer(Timer *timer) {
  timer->isTimerStart = true;
  if (!timer->isTimerPause) {
    timer->elapsedTime = GetTime() - timer->startTime;
  }
}

void PauseTimer(Timer *timer) { timer->isTimerPause = true; }

void ResetTimer(Timer *timer) {
  InitTimer(timer);
  timer->elapsedTime = 0;
}

float GetElapsedTime(Timer *timer) { return timer->elapsedTime; }