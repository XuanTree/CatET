#ifndef PLATFORM_H
#define PLATFORM_H

#pragma once
#include "player.h"
#include <raylib.h>

typedef enum PlatformType {
  SMALL,
  MEDIUM,
  LARGE,
  TOTAL_COUNT,
} PlatformType;

typedef struct Platform {
  Texture2D platformTexture;
  PlatformType platformType;
  Vector2 spawnPosition;
  Vector2 size;        // 贴图原生尺寸（绘制与碰撞的基准）
  float surfaceOffset; // 贴图顶部透明留白高度（像素）：碰撞顶面从可见表面起算
} Platform;

void InitJumpPlatforms(Platform *platform);
void LoadPlatformTexture(Platform *platform, PlatformType platformType);
void DrawPlatform(Platform *platform, PlatformType platformType);
void PlayerCollision(Player *player, Platform *platform, float dt);

#endif // PLATFORM_H
