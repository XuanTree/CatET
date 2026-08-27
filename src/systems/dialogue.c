#include "game.h"

typedef enum DialogueType {
  NONE,
  FOUR_TWO_FOUR,
  NO_IDEA,
  ENGLISH,
  SENTENCE_1,
  SENTENCE_2,
  SENTENCE_3,
  SENTENCE_4,
  SENTENCE_5,
  TIP,
  TOTAL_DIALOGUE_TYPES
} DialogueType;

const char *getDialogue() {
  int type = genRandomNum(TOTAL_DIALOGUE_TYPES);
  switch (type) {
  case NONE:
    return "...";
  case FOUR_TWO_FOUR:
    return "424, plz";
  case NO_IDEA:
    return "what are we doing?";
  case ENGLISH:
    return "i don't speak english";
  case SENTENCE_1:
    return "how many words can you remember?";
  case SENTENCE_2:
    return "dont choose the wrong wordss"; // 故意多打的一个s
  case SENTENCE_3:
    return "How much levels can you beat?"; // 故意弄错语法
  case SENTENCE_4:
    return "i don't know what to say";
  case SENTENCE_5:
    return "dodge the bullets, watch ur butts"; // 依旧故意的
  case TIP:
    return "tip: use the arrow keys to pick the answer";
  default:
    return " ";
  };
}