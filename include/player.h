#ifndef PLAYER_H
#define PLAYER_H

#include "animation.h"
#pragma once
#include "core/strings.h"
#include <raylib.h>

typedef enum PlayerAnimation { IDLE, RUN, JUMP, COUNT } PlayerAnimation;

typedef struct Player {
  int health;
  int maxHealth;
  Vector2 position;
  Vector2 velocity;
  Texture2D playerTexture;
  Vector2 size;
  Animation animations[COUNT];
  PlayerAnimation playerAnimationState;
  bool isOnTheGround;
} Player;

void InitPlayer(Player *player, String *spriteFilePath);
void UpdatePlayer(Player *player, float dt);
void DrawPlayer(Player *player);
void LoadPlayerTexture(Player *player, String *filePath);

#endif // PLAYER_H