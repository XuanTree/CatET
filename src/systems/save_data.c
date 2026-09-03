#include "game.h"
// 存档相对路径（拼接 GetApplicationDirectory()）。
// 资源已全部内嵌进可执行文件（不再部署 assets/ 目录），
// 存档单独写在可执行文件同级的 save.json，随安装目录持久化。
#define SAVE_PATH_REL "save.json"

// save.json 全部字段（新增字段须在此登记，读写统一走 LoadAll/SaveAll，
// 避免多个写入口互相覆盖）。
typedef struct SaveData {
  float bestTime;    // 最佳通关时间（秒），< 0 表示尚无记录
  int infiniteBest;  // 无尽模式单局最高答对数（0 表示尚无记录，
                     // 独立于主线速通，见 scene_infinite）
  bool soundEnabled; // 音效总开关
  bool musicEnabled; // 音乐总开关
} SaveData;

static const char *SavePath(void) {
  return TextFormat("%s%s", GetApplicationDirectory(), SAVE_PATH_REL);
}

// 解析 JSON 布尔字段（"true"/"false"）；找不到或格式异常返回 fallback。
// 为什么手写解析：存档仅少量标量字段，引入 JSON 库纯属过度设计；
// strstr 定位键名 + 冒号后取值的写法与既有 bestTime 解析保持一致。
static bool ParseBoolField(const char *json, const char *key, bool fallback) {
  const char *pos = strstr(json, key);
  if (pos == NULL)
    return fallback;
  const char *colon = strchr(pos, ':');
  if (colon == NULL)
    return fallback;
  // 跳过冒号后的空白
  const char *value = colon + 1;
  while (*value == ' ' || *value == '\t' || *value == '\r' ||
         *value == '\n') {
    value++;
  }
  if (strncmp(value, "true", 4) == 0)
    return true;
  if (strncmp(value, "false", 5) == 0)
    return false;
  return fallback;
}

// 解析 JSON 整数字段（冒号后的整数）；找不到或格式异常返回 fallback。
static int ParseIntField(const char *json, const char *key, int fallback) {
  const char *pos = strstr(json, key);
  if (pos == NULL)
    return fallback;
  const char *colon = strchr(pos, ':');
  if (colon == NULL)
    return fallback;
  const char *value = colon + 1;
  while (*value == ' ' || *value == '\t' || *value == '\r' ||
         *value == '\n') {
    value++;
  }
  const int v = atoi(value);
  return (v >= 0) ? v : fallback; // 非法负数视为格式损坏
}

// 全量读取存档；文件缺失或格式损坏时各字段回退默认值（bestTime 为 -1，
// infiniteBest 为 0，音频开关取 game_config 的 DEFAULT_*），保证任何旧版本
// 存档都能启动（旧存档无 infiniteBest 字段时自然回落为 0）。
static SaveData SaveDataLoadAll(void) {
  SaveData data = {
      .bestTime = -1.0f,
      .infiniteBest = 0,
      .soundEnabled = DEFAULT_SOUND_ENABLED,
      .musicEnabled = DEFAULT_MUSIC_ENABLED,
  };
  const char *path = SavePath();
  FILE *fp = fopen(path, "rb");
  if (fp == NULL)
    return data; // 尚无存档，返回全默认
  char buf[256];
  size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
  fclose(fp);
  buf[n] = '\0';

  // 简单解析 JSON 字段 {"bestTime": 123.45, "infiniteBest": 20, ...}：
  // 定位键名后的冒号再取数字（与旧版 bestTime 解析兼容）
  const char *key = strstr(buf, "bestTime");
  if (key != NULL) {
    const char *colon = strchr(key, ':');
    if (colon != NULL) {
      const float v = (float)atof(colon + 1);
      data.bestTime = (v >= 0.0f) ? v : -1.0f;
    }
  }
  data.infiniteBest = ParseIntField(buf, "infiniteBest", 0);
  data.soundEnabled = ParseBoolField(buf, "soundEnabled",
                                     DEFAULT_SOUND_ENABLED);
  data.musicEnabled = ParseBoolField(buf, "musicEnabled",
                                     DEFAULT_MUSIC_ENABLED);
  return data;
}

// 全量写入存档（所有字段一次写出）。
static void SaveDataSaveAll(const SaveData *data) {
  FILE *fp = fopen(SavePath(), "wb");
  if (fp == NULL)
    return; // 写失败静默降级（不崩溃），下次运行再读旧值
  fprintf(fp, "{\"bestTime\": %.2f, \"infiniteBest\": %d, "
              "\"soundEnabled\": %s, \"musicEnabled\": %s}\n",
          data->bestTime, data->infiniteBest,
          data->soundEnabled ? "true" : "false",
          data->musicEnabled ? "true" : "false");
  fclose(fp);
}

float SaveDataLoadBestTime(void) {
  return SaveDataLoadAll().bestTime;
}

void SaveDataSaveBestTime(float bestTime) {
  if (bestTime < 0.0f)
    return;
  // 先读全量再只改 bestTime，避免覆盖玩家已设置的音频开关/无尽纪录
  SaveData data = SaveDataLoadAll();
  data.bestTime = bestTime;
  SaveDataSaveAll(&data);
}

int SaveDataLoadInfiniteBest(void) {
  const int v = SaveDataLoadAll().infiniteBest;
  return (v > 0) ? v : 0;
}

bool SaveDataSaveInfiniteBest(int best) {
  if (best <= 0)
    return false;
  // 先读全量再只改 infiniteBest，避免覆盖已持久化的 bestTime / 音频设置
  SaveData data = SaveDataLoadAll();
  if (best <= data.infiniteBest)
    return false;
  data.infiniteBest = best;
  SaveDataSaveAll(&data);
  return true;
}

bool SaveDataLoadSoundEnabled(void) {
  return SaveDataLoadAll().soundEnabled;
}

bool SaveDataLoadMusicEnabled(void) {
  return SaveDataLoadAll().musicEnabled;
}

void SaveDataSaveSettings(bool soundEnabled, bool musicEnabled) {
  // 先读全量再只改音频开关，避免覆盖已持久化的最佳通关时间/无尽纪录
  SaveData data = SaveDataLoadAll();
  data.soundEnabled = soundEnabled;
  data.musicEnabled = musicEnabled;
  SaveDataSaveAll(&data);
}
