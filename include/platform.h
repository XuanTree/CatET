#ifndef PLATFROM_H
#define PLATFROM_H

#pragma once
#include <raylib.h>

typedef struct Platform {
  Texture2D platformTexture;
  Vector2 spawnPosition;
} Platform;

void LoadPlatformTexture(Platform *platform);

#endif // PLATFROM_H