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

// ── 弹幕 pattern（敌怪攻击模板）──────────────────────────────────────────
// 敌怪每次攻击随机选择一种 pattern，数量也在区间内随机，增加玩法多样性。
typedef enum BulletPattern {
  BULLET_PATTERN_AIMED = 0, // 瞄准扇形：向目标方向散射
  BULLET_PATTERN_RING,      // 环形：以发射点为中心 360° 均匀分布
  BULLET_PATTERN_RAIN,      // 弹幕雨：宽扇形向下倾泻
  BULLET_PATTERN_CROSS,     // 十字斜扫：沿对角方向发射
  BULLET_PATTERN_COUNT,
} BulletPattern;

// 随机返回一种弹幕 pattern（保证每次攻击有变化）。
BulletPattern BulletPatternRoll(void);

// 按 pattern 从 origin 生成 count 颗弹幕写入 bullets 数组（每颗独立加载贴图，
// 复用槽位前会先释放旧贴图），返回实际生成数（count 会被钳制到 maxBullets）。
// target 供瞄准类 pattern 使用（决定散射中心方向）。
int BulletPatternFire(Bullet *bullets, int maxBullets, BulletPattern pattern,
                      Vector2 origin, Vector2 target, int count, float speed,
                      float damage);

#endif // BULLET_H
