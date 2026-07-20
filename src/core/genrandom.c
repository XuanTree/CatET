#include <core/genrandom.h>

int genRandomNumber(int range) {
  srand(time(NULL));

  if (range <= 0)
    return 0;
  return rand() % range;
}