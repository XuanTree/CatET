#ifndef PLATFORM_H
#define PLATFORM_H

#pragma once
#include <raylib.h>

typedef struct Platform {
  Texture2D platformTexture;
  Vector2 spawnPosition;
} Platform;

void LoadPlatformTexture(Platform *platform);

#endif // PLATFORM_H
