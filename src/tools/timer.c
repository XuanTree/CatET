#include "game.h"

void InitTimer(Timer *timer) {
  timer->isTimerStart = false;
  timer->isTimerPause = false;
  timer->startTime = GetTime();
  timer->elapsedTime = 0; // 必须清零，否则未初始化读取属未定义行为
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

// 状态机：state 记录「最近一次已同步/已提示的剩余整秒刻度」（初始 0）。
// 倒计时从 warnAt 秒以上递减进入窗口时，首次跨过 warnAt 整秒与之后每跨过
// 下一个整秒（warnAt-1、…、1）各返回一次 true——即最后 warnAt 秒的每一秒
// 触发一次（如剩余 5、4、3、2、1 秒整时）。remaining 归零复位 state；时间
// 重置到更大值时同步刻度而不触发（自动重新武装），下一轮递减再次逐秒触发。
// 刻度用向上取整（ceilf），与“还剩整整 X 秒”的时刻对齐，不依赖固定帧率。
bool TimerCountdownWarn(int *state, float remaining, float warnAt) {
  if (!state)
    return false;
  if (remaining <= 0.0f) { // 倒计时归零/结束：复位，等待下一轮
    *state = 0;
    return false;
  }
  const int cur = (int)ceilf(remaining); // 剩余整秒刻度（如剩余 4.2s → 5）
  if (remaining > warnAt) {
    // 尚未进入警告窗口：同步刻度而不触发（窗口外也算重武装）
    *state = cur;
    return false;
  }
  // ── 窗口内（remaining <= warnAt）──
  if (*state == 0) { // 首次进入窗口（此前从未同步）：立即提示一次
    *state = cur;
    return true;
  }
  if (cur >= *state) { // 时间未减少 / 窗口内重置到更大值：不触发
    *state = cur;
    return false;
  }
  *state = cur; // 整秒刻度下降：本秒首次触发
  return true;
}

