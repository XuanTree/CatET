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
  SENTENCE_6,
  SENTENCE_7,
  SENTENCE_8,
  SENTENCE_9,
  SENTENCE_10,
  TIP,
  TIPPP,
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
    return "please!!! dodge the bullets"; // 依旧故意的
  case SENTENCE_6:
    return "to be or not to be";
  case SENTENCE_7:
    return "whats the point?";
  case SENTENCE_8:
    return "cet isnt that hard you imagine";
  case SENTENCE_9:
    return "again? im tired...";
  case SENTENCE_10:
    return "15? 17?";
  case TIP:
    return "tip: use the arrow keys to pick the answer";
  case TIPPP:
    return "when your hp reach 0, you win~";
  default:
    return " ";
  };
}

typedef enum StartTextType {
  QUOTE_1,
  QUOTE_2,
  QUOTE_3,
  QUOTE_4,
  QUOTE_5,
  QUOTE_6,
  QUOTE_7,
  QUOTE_8,
  QUOTE_9,
  QUOTE_10,
  QUOTE_11,
  QUOTE_12,
  QUOTE_13,
  QUOTE_14,
  QUOTE_15,
  QUOTE_TOTAL
} StartTextType;

const char *getStartText() {
  int type = genRandomNum(QUOTE_TOTAL);
  switch (type) {
  case QUOTE_1:
    return "“Learning is not attained by chance; it must be sought for with "
           "ardor and attended to with diligence.” — Abigail Adams";
  case QUOTE_2:
    return "“The expert in anything was once a beginner.” — Helen Hayes";
  case QUOTE_3:
    return "“Study while others are sleeping; work while others are loafing; "
           "prepare while others are playing; and dream while others are "
           "wishing.” — William Arthur Ward";
  case QUOTE_4:
    return "“The roots of education are bitter, but the fruit is sweet.” — "
           "Aristotle";
  case QUOTE_5:
    return "“The mind is not a vessel to be filled, but a fire to be "
           "kindled.” — Plutarch";
  case QUOTE_6:
    return "“Education is not preparation for life; education is life "
           "itself.” — John Dewey";
  case QUOTE_7:
    return "“The only person who is educated is the one who has learned how "
           "to learn and change.” — Carl Rogers";
  case QUOTE_8:
    return "“I am still learning.” — Michelangelo";
  case QUOTE_9:
    return "“Never stop learning, because life never stops teaching.” — "
           "Anonymous";
  case QUOTE_10:
    return "“The more I read, the more I acquire, the more certain I am that "
           "I know nothing.” — Voltaire";
  case QUOTE_11:
    return "“Success is the sum of small efforts, repeated day in and day "
           "out.” — Robert Collier";
  case QUOTE_12:
    return "“It always seems impossible until it’s done.” — Nelson Mandela";
  case QUOTE_13:
    return "“Don’t let what you cannot do interfere with what you can do.” — "
           "John Wooden";
  case QUOTE_14:
    return "“Knowledge is power.” — Francis Bacon";
  case QUOTE_15:
    return "“Anyone who stops learning is old, whether at twenty or eighty. "
           "Anyone who keeps learning stays young.” — Henry Ford";
  default:
    return " ";
  }
}

// TODO Write A Story For The Game...To make it more interestring...
// 简单难度和普通难度共用一个故事模板
const char *getStory(int difficulty, int level) {
  switch (difficulty) {
  case 0: // 简单难度
  case 1: // 普通难度
    switch (level) {
    case 1:
      return "Once upon a time, there was a little cat named cat...";
    default:
      return "There must be someting wrong...";
    }
  case 2: // 困难难度
    switch (level) {
    case 1:
      return "huh? Hard Mode...";
    default:
      return "There must be something, hahaha, wrong!!!";
    }
  default:
    return "what?";
  }
  return "The End";
}
