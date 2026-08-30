/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#pragma once

// 全局统一缩放：玩家与平台共用同一缩放，保证绘制与碰撞处于相同比例，
// 避免不同物体缩放不一致导致“悬浮 / 贴图不吻合”的观感。
#define GAME_SCALE 3.0f
#define TRANSITION_SECONDS 0.45f

// 总关卡数：玩家通关第 MAX_LEVELS 关判定「最终胜利」（成功通关），
// 此时停止隐式全局计时器并记录最佳通关时间（见 systems/speedrun）。
#define MAX_LEVELS 100

// ─────────────────────────────────────────────────────────────────────────────
// 数值中心化
//   所有跨场景平衡数值集中在本文档，各场景禁止再出现魔法数字。
//   设计原则（2026-08 校核）：惩罚温和化、回血随关卡递增、避免“慢性死亡”。
// ─────────────────────────────────────────────────────────────────────────────

// ── 玩家 / 生命经济 ──────────────────────────────────────────────────────────
// 最大生命值基础值；Hard 难度额外 +25%（=125），Easy/Normal 为 100。
// 由 PlayerApplyDifficulty 在场景 Enter 按难度应用（见 entities/player）。
#define PLAYER_MAX_HEALTH_BASE 100.0f
#define PLAYER_MAX_HEALTH_HARD_MULT 1.25f // Hard 血上限 +25%（=125）

// 通关奖励随关卡递增（普通关 5 + level*0.1 → 第1关5、第100关15；
// boss 关 20 + level*0.04 → 20~24）。用函数式宏，场景传 level 计算。
#define CLEAR_HEALTH_REWARD_BASE 5.0f
#define CLEAR_HEALTH_REWARD_PER_LEVEL 0.1f
#define BOSS_CLEAR_HEALTH_REWARD_BASE 20.0f
#define BOSS_CLEAR_HEALTH_REWARD_PER_LEVEL 0.04f
#define ClearHealthReward(level)                                               \
  (CLEAR_HEALTH_REWARD_BASE + (float)(level) * CLEAR_HEALTH_REWARD_PER_LEVEL)
#define BossClearHealthReward(level)                                           \
  (BOSS_CLEAR_HEALTH_REWARD_BASE +                                             \
   (float)(level) * BOSS_CLEAR_HEALTH_REWARD_PER_LEVEL)

// 掉落惩罚：maxHP 的 15%（100→15，Hard 125→18.75）。超时/拼错惩罚统一为 15
// （学习友好：温和，配合通关回血递增避免慢性死亡）。
#define FALL_PENALTY_RATIO 0.15f
#define TIME_PENALTY 15.0f
#define SPELL_WRONG_PENALTY 15.0f
#define MAZE_WRONG_PENALTY 15.0f

// ── 战斗（三选一，scene_battle）─────────────────────────────────────────────
// 拼写错误惩罚：Easy 20 / Normal 25 / Hard 30（由 1.5^d 改为 1.25^d 取整，
// 降低难度陡峭度）；弹幕伤害浮动 3~8（Hard 波次多，上限 9→8 补偿）。
#define BATTLE_WRONG_PENALTY_EASY 20.0f
#define BATTLE_WRONG_PENALTY_NORMAL 25.0f
#define BATTLE_WRONG_PENALTY_HARD 30.0f
#define BATTLE_BULLET_DMG_MIN 3.0f
#define BATTLE_BULLET_DMG_MAX 8.0f
// 战斗胜利所需答对单词数：按难度收窄随机区间（降低单场强度方差）
#define BATTLE_ROUNDS_EASY_MAX 3   // easy 1~3
#define BATTLE_ROUNDS_NORMAL_MIN 2 // normal 2~4
#define BATTLE_ROUNDS_NORMAL_MAX 4
#define BATTLE_ROUNDS_HARD_MIN 2 // hard 2~4
#define BATTLE_ROUNDS_HARD_MAX 4

// ── Boss 战（scene_bossfight）──────────────────────────────────────────────
// Boss 拼错惩罚（仅困难生效）：与战斗 Hard 一致
#define BOSS_WRONG_PENALTY_HARD 30.0f

// ── 音频总开关默认值：首次运行或旧存档缺少字段时使用（设置界面可修改，
// 持久化到 save.json，见 systems/save_data）。
#define DEFAULT_SOUND_ENABLED true
#define DEFAULT_MUSIC_ENABLED true

#endif // GAME_CONFIG_H
