#ifndef CHARACTER_H
#define CHARACTER_H

#pragma once
#include "player.h"
#include "tools/strings.h"
#include <raylib.h>

typedef struct Character {
  Rectangle charRectangle;
  String character;
  String answer;
  bool isGravityOn;

} Character;

// 当玩家与字母hitbox重合时，玩家按z可以拿起字母（游戏表现为把字母顶脑袋上）。
// 玩家再次按Z可以放下字母，字母（或者单词）受重力影响
// 游戏画面中，优先绘制字母，再绘制玩家，确保玩家再图层最上方
void CharCollision(Player *player, float dt);
// 字母碰到迷宫边界时（届时迷宫由Rectangle绘制），停止下落，isGravityOn设为false。
void BoundCollision(Rectangle *bound, float dt);
// 检查玩家拾取字母并组成的单词是否正确
void WordCheck(const char *answer, float dt);

#endif // !CHARACTER_H