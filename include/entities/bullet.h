#ifndef BULLET_H
#define BULLET_H

#pragma once
#include <raylib.h>
#include <stdbool.h>

// boss 发射的弹幕(bullet)：对玩家造成碰撞伤害，碰撞造成伤害后删除 bullet。
// isActive 标记是否处于飞行状态（false 表示待销毁/可复用）。
typedef struct Bullet {
  Vector2 position;        // 世界坐标左上角
  Vector2 velocity;        // 世界坐标速度
  Vector2 size;            // 世界坐标尺寸（贴图尺寸 × GAME_SCALE）
  float damage;            // 对玩家造成的碰撞伤害
  float lifetime;          // 存活时长（秒），递减至 <= 0 时自动失效
  bool isActive;           // 是否处于飞行状态
  Texture2D bulletTexture; // assets/sprites/bullet.png
} Bullet;

// 初始化子弹：设定出生位置、速度与伤害，置 isActive
void InitBullet(Bullet *bullet, Vector2 position, Vector2 velocity,
                float damage);

// 每帧更新：位移、衰减存活时长，超时自动失效
void UpdateBullet(Bullet *bullet, float dt);

// 绘制子弹
void DrawBullet(Bullet *bullet);

#endif // BULLET_H
