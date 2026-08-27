#ifndef PLATFORM_H
#define PLATFORM_H

#pragma once
#include "entities/player.h"
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
  Vector2 size;        // 世界坐标尺寸（贴图尺寸 × GAME_SCALE，绘制与碰撞共用）
  float surfaceOffset; // 顶部透明留白高度（世界坐标）：碰撞顶面从可见表面起算
} Platform;

void InitJumpPlatforms(Platform *platform, Vector2 spawnPosition,
                       PlatformType platformType);
void DrawPlatform(Platform *platform);
void PlayerCollision(Player *player, Platform *platform);

#endif // PLATFORM_H
