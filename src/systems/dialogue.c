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

// 简单难度和普通难度共用一个故事模板
const char *getStory(int difficulty, int level) {
  switch (difficulty) {
  case 0: // 简单难度
  case 1: // 普通难度
    switch (level) {
    case 1:
      return "Once upon a time, there was a little cat named cat...";
    case 2:
      return "cat was very cute, but he was not very smart...";
    case 3:
      return "The only language cat could speak was MEOW !!!";
    case 4:
      return "He thought: I only heard MEOW in my life, so everybody speaks "
             "MEOW...";
    case 5:
      return "But he was wrong";
    case 6:
      return "One Day, he heard something, something that was not MEOW...";
    case 7:
      return "How Amazing!";
    case 8:
      return "......";
    case 9:
      return "Cat finally realized that there are many languages in the world";
    case 10:
      return "Meow is not the only language!";
    case 11:
      return "Cat was Very Happy!";
    case 12:
      return "\"I found another language in the world!\"";
    case 13:
      return "But, what language is it?";
    case 14:
      return "Cat was very curious...";
    case 15:
      return "He decided to find out!";
    case 16:
      return "Cat went out into the world to find out...";
    case 17:
      return "And finally he learned that language is called English";
    case 18:
      return "Cat immediately decided!";
    case 19:
      return "\"I want to learn ENGLISH!!!\"";
    case 21:
      return "Cat went to school to learn English...";
    case 22:
      return "But, he was not very good at learning...";
    case 23:
      return "Cat was very sad...";
    case 24:
      return "But, he was not giving up...";
    case 25:
      return "\"The only thing I need to do is to try much harder!\"";
    case 26:
      return "Cat tried harder and harder...";
    case 27:
      return "In fact, Cat totally didn't know how to learn English.";
    case 28:
      return "The only thing he really did was memorize words and...";
    case 29:
      return "Imitate someone's speech!";
    case 30:
      return "Cat practiced day and night...";
    case 31:
      return "Cat's English gradually improved a lot without notice...";
    case 32:
      return "With Cat's age growing, Cat went to high school.";
    case 33:
      return "The way school taught English was quite different";
    case 34:
      return "Different from how Cat learned English himself!";
    case 35:
      return "It's boring.";
    case 36:
      return "It's high scores first";
    case 37:
      return "It's only focusing on some sort of problem solving...";
    case 38:
      return "That's Not English!!!";
    case 39:
      return "It's not ANY language";
    case 41:
      return "Learning a language is NOT only solving stupid questions on "
             "paper";
    case 42:
      return "It's Reading interesting stories";
    case 43:
      return "It's Writing humorous articles";
    case 44:
      return "It's Hearing voices that come from other countries";
    case 45:
      return "It's Speaking our own feelings to others";
    case 46:
      return "It's Learning other cultures in the world!";
    case 47:
      return "...";
    case 48:
      return "Why can so many people still not learn English well after "
             "graduation?";
    case 49:
      return "Cat thought a lot a lot...";
    case 50:
      return "It's not because English is that hard";
    case 51:
      return "Hey, I promise that English is much easier than Advanced "
             "Mathematics";
    case 52:
      return "So the reality is,";
    case 53:
      return "The way school taught us English was WRONG";
    case 54:
      return "Many treat learning English as torture;";
    case 55:
      return "They didn't see the true beauty behind languages";
    case 56:
      return "If you can learn your native language well...";
    case 57:
      return "So can you learn other languages.";
    case 58:
      return "Cat started to think...";
    case 59:
      return "\"How can i tell others that English is fun?\"";
    case 61:
      return "...";
    case 62:
      return "And finally, he came up with an idea";
    case 63:
      return "Why don't we make a game?";
    case 64:
      return "Cat endured boring studies in school";
    case 65:
      return "When Cat finally went to college";
    case 66:
      return "Cat learned that there was a hard test called CET";
    case 67:
      return "So what does CET mean?";
    case 68:
      return "And why are so many beaten by it?";
    case 69:
      return "To save others, Cat decided...";
    case 70:
      return "to take a journey to learn what CET exactly is!";
    case 71:
      return "\"Hmmm, how can we beat CET???\"";
    case 72:
      return "Words are the first";
    case 73:
      return "In fact, the majority of the time we spend on learning a "
             "language";
    case 74:
      return "is memorizing words!";
    case 75:
      return "so, why not focus on the words first?";
    case 76:
      return "Typically, there are no shortcuts to memorizing words.";
    case 77:
      return "so just keep testing yourself!";
    case 78:
      return "using these words that are collected for you...";
    case 79:
      return "Go! With Cat's Power!!!";
    case 81:
      return "20 levels left! Can you hold on to the last level?";
    default:
      // Boss 关（第 20/40/60/80 关）与最终关（第 100 关）没有剧情：
      // 返回空串，剧情系统（systems/story）据此不显示文本框。
      return "";
    }
  case 2: // 困难难度
    switch (level) {
    case 1:
      return "huh? Hard Mode...";
    case 5:
      return "You must have passed CET-4!";
    case 10:
      return "Go ahead for CET-6!";
    default:
      // 困难难度仅在少量关卡提供剧情，其余关卡无剧情（空串 = 不显示）
      return "";
    }
  default:
    return "";
  }
  return "";
}
