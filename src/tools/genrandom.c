#include <tools/genrandom.h>

static int genRandomNumber(const int range) {
  srand(time(NULL));

  if (range <= 0)
    return 0;
  return rand() % range;
}