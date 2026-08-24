#include "tools/genrandom.h"
#include <stdbool.h>

// 随机数只需在进程内播种一次（code_style §11 要求：避免 srand(time(NULL))
// 每次调用重播导致同一秒内多次调用得到相同序列）。用静态标志保证仅播种一次。
static bool s_seeded = false;

static void EnsureSeeded(void) {
  if (!s_seeded) {
    srand((unsigned int)time(NULL));
    s_seeded = true;
  }
}

// 返回 [0, range) 区间内的随机整数；range <= 0 时返回 0（安全值）。
int genRandomNum(int range) {
  EnsureSeeded();
  if (range <= 0)
    return 0;
  return rand() % range;
}
