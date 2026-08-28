#include "game.h"

// 说明：EmbeddedAssetGet 的实现由 src/tools/pack_assets.py 生成并链接进
// 可执行文件（generated/embedded_assets.c）。其原型已在
// include/tools/resource.h 中声明（本文件经 game.h 间接引入），因此无需 include
// 构建目录下的生成头 文件，避免 IDE/静态分析在编译配置之外报 "embedded_assets.h
// not found"。

// 由路径提取扩展名（含点，如 ".png"）；无扩展名返回空串，保证
// raylib 的 Load*FromMemory 的 fileType 参数永不为 NULL（避免解引用崩溃）。
static const char *AssetExt(const char *path) {
  const char *dot = path ? strrchr(path, '.') : NULL;
  return dot ? dot : "";
}

Image LoadEmbeddedImage(const char *path) {
  size_t size = 0;
  const unsigned char *data = EmbeddedAssetGet(path, &size);
  if (!data || size == 0)
    return (Image){0};
  return LoadImageFromMemory(AssetExt(path), data, (int)size);
}

Texture2D LoadEmbeddedTexture(const char *path) {
  Image img = LoadEmbeddedImage(path);
  if (!img.data)
    return (Texture2D){0};
  Texture2D tex = LoadTextureFromImage(img);
  UnloadImage(img);
  return tex;
}

Sound LoadEmbeddedSound(const char *path) {
  size_t size = 0;
  const unsigned char *data = EmbeddedAssetGet(path, &size);
  if (!data || size == 0)
    return (Sound){0};
  Wave wave = LoadWaveFromMemory(AssetExt(path), data, (int)size);
  if (wave.frameCount == 0)
    return (Sound){0};
  Sound sound = LoadSoundFromWave(wave);
  UnloadWave(wave);
  return sound;
}

Font LoadEmbeddedFontEx(const char *path, int fontSize, int *codepoints,
                        int cpCount) {
  size_t size = 0;
  const unsigned char *data = EmbeddedAssetGet(path, &size);
  if (!data || size == 0)
    return (Font){0};
  return LoadFontFromMemory(AssetExt(path), data, (int)size, fontSize,
                            codepoints, cpCount);
}
