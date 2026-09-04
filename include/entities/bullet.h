/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

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

// 弹幕的命中判定圆（视觉内容圆）：写回圆心（世界坐标）与半径。
// 弹幕贴图为 16×16 内居中的 12×12 圆形内容，绘制/离屏/消失判定仍用整幅
// 方盒 size（24×24）；但命中若也用方盒，方形四角会在斜向接近时先于视觉
// 球体触发命中，观感“还没碰到就受伤”。命中判定请用本接口返回的圆配合
// 玩家受击盒 PlayerHitRect 做圆-矩形检测。
void BulletHitCircle(const Bullet *bullet, Vector2 *outCenter, float *outRadius);

// ── 弹幕 pattern（敌怪攻击模板）──────────────────────────────────────────
// 敌怪每次攻击随机选择一种 pattern（共 10 种），数量也在区间内随机，增加玩法
// 多样性。均由「初始速度/位置排布」构成，无需逐帧特殊状态。
typedef enum BulletPattern {
  BULLET_PATTERN_AIMED = 0,  // 瞄准扇形：窄扇形朝向玩家
  BULLET_PATTERN_RING,       // 环形：360° 一圈
  BULLET_PATTERN_RAIN,       // 弹幕雨：向下宽扇形倾泻
  BULLET_PATTERN_SPIRAL,     // 螺旋：角度随序号递增的螺旋射线
  BULLET_PATTERN_PINCER,     // 钳形：朝玩家左右两侧各一组扇形夹击
  BULLET_PATTERN_WALL,       // 弹幕墙：横向一排平行弹幕扫射
  BULLET_PATTERN_STAR,       // 星形：6 轴放射（每 60°）
  BULLET_PATTERN_AIMED_WIDE, // 瞄准大扇形：宽角朝向玩家
  BULLET_PATTERN_TRIPLE_ARC, // 三段弧：朝玩家左/中/右三组齐射
  BULLET_PATTERN_HAIL,       // 天降：屏幕上方多点竖直下落
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

// 弹幕浮动伤害：每颗弹幕命中随机造成 3~9 点伤害（增加战斗随机性，
// 不随难度缩放；由各场景在发射后逐颗覆写 damage）。
float BulletRollDamage(void);

#endif // BULLET_H
