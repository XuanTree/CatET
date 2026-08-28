#include "game.h"
// 存档相对路径（拼接 GetApplicationDirectory()）。
// 资源已全部内嵌进可执行文件（不再部署 assets/ 目录），
// 存档单独写在可执行文件同级的 save.json，随安装目录持久化。
#define SAVE_PATH_REL "save.json"

static const char *SavePath(void) {
  return TextFormat("%s%s", GetApplicationDirectory(), SAVE_PATH_REL);
}

float SaveDataLoadBestTime(void) {
  const char *path = SavePath();
  FILE *fp = fopen(path, "rb");
  if (fp == NULL)
    return -1.0f; // 尚无存档
  char buf[256];
  size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
  fclose(fp);
  buf[n] = '\0';

  // 简单解析 JSON 字段 {"bestTime": 123.45}：定位键名后的冒号再取数字
  const char *key = strstr(buf, "bestTime");
  if (key == NULL)
    return -1.0f;
  const char *colon = strchr(key, ':');
  if (colon == NULL)
    return -1.0f;
  const float v = (float)atof(colon + 1);
  return (v >= 0.0f) ? v : -1.0f;
}

void SaveDataSaveBestTime(float bestTime) {
  if (bestTime < 0.0f)
    return;
  FILE *fp = fopen(SavePath(), "wb");
  if (fp == NULL)
    return; // 写失败静默降级（不崩溃），下次运行再读旧值
  fprintf(fp, "{\"bestTime\": %.2f}\n", bestTime);
  fclose(fp);
}
