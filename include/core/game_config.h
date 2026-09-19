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

// 倒计时剩余警告阈值（秒）：所有带 HUD 倒计时的关卡（平台/极速拼写/迷宫）
// 剩余时间进入最后该秒数后，每跨一个整秒播放一次 tick.ogg（见 tools/timer 的
// TimerCountdownWarn 与各场景 Update 调用）：跨过 5、4、3、2、1 秒整各一声。
#define COUNTDOWN_WARN_SECONDS 5.0f

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

// ── 无尽模式（scene_infinite）────────────────────────────────────────────
// 「无敌人的战斗场景」：黑底网格舞台 + 每轮三选一（词库/词性干扰规则与
// 战斗一致，给词性+中文释义选英文单词）。答对仅计 1 分、不回复生命值
// （答对是继续游戏的前提，失误次数是资源），答错按难度扣血且把词条送入
// 错词本（study_tracker：拼错后间隔 STUDY_REVISIT_INTERVAL 题再复现，
// 驱动复习）；HP 归零本局结束，以「单局最高答对数」作为无尽模式最佳成绩，
// 独立于主线速通持久化（见 systems/save_data 的 infiniteBest 字段）。
#define INFINITE_WRONG_PENALTY_EASY 15.0f   // Easy：100HP 下约 6 次失误出局
#define INFINITE_WRONG_PENALTY_NORMAL 20.0f // Normal：100HP 下约 5 次失误出局
#define INFINITE_WRONG_PENALTY_HARD 25.0f   // Hard：125HP 下约 5 次失误出局
#define INFINITE_REVIEW_SECONDS 3.0f // 答错后正确答案复习横幅显示时长（秒）
#define INFINITE_CORRECT_FLASH_SECONDS 1.0f // 答对后得分反馈横幅显示时长（秒）

// ── 平台关卡（scene_platform）：布局 / 手感 / 引导集中调参 ────────────────
// 关卡为「垂直爬塔 + 水平跳跃 + 下落」随机混合，每关随机朝左或朝右推进，
// 世界尺寸按实际占用推算（可远超一屏），相机跟随滚动。
#define PLATFORM_MAX_PLATFORMS 14     // 单关平台数量上限
#define PLATFORM_SPAWN_CLEAR 240.f    // 起点地面两侧「禁生成平台」保护距离
#define PLATFORM_CLIMB_DX 150.f       // 攀爬步最大左右错位
#define PLATFORM_CLIMB_RISE_MIN 120.f // 攀爬步最小上升（拉大纵向间距）
#define PLATFORM_CLIMB_RISE_MAX 190.f // 攀爬步最大上升（跳跃可达上限）
#define PLATFORM_RUN_DX_MIN 230.f     // 横跳步最小推进（拉大横向间距）
#define PLATFORM_RUN_DX_MAX 380.f     // 横跳步最大推进（水平跳距内）
#define PLATFORM_RUN_DY 70.f          // 横跳步垂直起伏（±）
#define PLATFORM_DROP_DX_MIN 200.f    // 下落步最小推进
#define PLATFORM_DROP_DX_MAX 340.f    // 下落步最大推进
#define PLATFORM_DROP_DY_MIN 110.f    // 下落步最小下降
#define PLATFORM_DROP_DY_MAX 190.f    // 下落步最大下降
#define PLATFORM_PATH_TOP (-1500.f)   // 路线最高处（平台 spawn y 下限）
#define PLATFORM_PATH_BOTTOM 520.f    // 路线最低处（平台 spawn y 上限）
#define PLATFORM_WORLD_MARGIN 140.f   // 世界四周预留边距
// 坠落判定：y 超过路线最低点再下坠该距离即视为坠落（回最近落脚点并扣血）
#define PLATFORM_FALL_DEATH_Y (PLATFORM_PATH_BOTTOM + 300.f)
// 平台间距在两平台半宽之和之外追加的净间隙（加大以减轻视觉拥挤）
#define ROW_GAP_MARGIN 40.f
// 平台贴图统一 16px 高 → 世界坐标 48（assets/sprites/platform_*.png 均 16 高）
#define PLATFORM_BOX_H (16.f * GAME_SCALE)
// 距出生点最远的平台放置红旗；LARGE 大平台作为节奏锚点定期出现
#define PLATFORM_LARGE_EVERY 5      // 每 N 块放一块 LARGE（半宽 192）大平台
#define PLATFORM_CHECKPOINT_EVERY 4 // 每 N 块设一个检查点（坠落回到最近检查点）
#define PLATFORM_CHECKPOINT_HEAL 8.0f // 抵达新检查点的小幅回血
#define PLATFORM_NARROW_WIDTH 96.f    // 「窄平台」宽度阈值（SMALL 平台宽度）
#define PLATFORM_NARROW_STEP_SCALE                                             \
  0.62f // 窄平台后一步的水平推进缩放（助跑空间）
// 跳跃可达估算（与 entities/player.c 的物理常量保持一致，用于生成期校验）
#define PLATFORM_GRAVITY 980.f      // 与 player.c GRAVITY 一致
#define PLATFORM_JUMP_V0 580.f      // 与 player.c JUMP_SPEED 绝对值一致
#define PLATFORM_RUN_SPEED 384.f    // 与 player.c RUN 速度一致（240 × 1.6）
#define PLATFORM_REACH_MARGIN 0.85f // 可达性裕度（取理论跳距的 85%）
// 关卡限时：基础时长 + 实际路径长度 / 参考速度 × 裕度
#define PLATFORM_TIME_BASE 40.0f   // 基础限时（秒）
#define PLATFORM_TIME_SPEED 150.f  // 路径长度→时间的参考速度（px/s）
#define PLATFORM_TIME_MARGIN 1.35f // 限时裕度
// 音频总开关默认值：首次运行或旧存档缺少字段时使用（设置界面可修改，
// 持久化到 save.json，见 systems/save_data）。
#define DEFAULT_SOUND_ENABLED true
#define DEFAULT_MUSIC_ENABLED true

#endif // GAME_CONFIG_H
