/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef GAMEAPP_H
#define GAMEAPP_H

#pragma once
#include <math.h>
#include <raylib.h>

// 前置声明（完整定义见 systems/study_tracker.h；game.h 已包含）。
// 仅用于 GameApp.study 指针字段，避免 core 层反向依赖 systems 层。
typedef struct StudyTracker StudyTracker;

// ─────────────────────────────────────────────────────────────────────────────
// 框架层：统一管理窗口、图标、音频、固定分辨率渲染目标与呈现逻辑。
// 所有场景复用同一套「固定分辨率渲染 + 等比缩放呈现」流程，
// 缩放、居中、黑边、全屏切换集中在 GameAppPresent，一处改动全局生效。
// ─────────────────────────────────────────────────────────────────────────────

typedef struct GameApp {
  int logicWidth;       // 逻辑分辨率宽（固定）
  int logicHeight;      // 逻辑分辨率高（固定）
  RenderTexture target; // 固定分辨率渲染目标，避免放大后画面模糊
  bool isPaused;
  // 跨关卡继承的玩家生命值：关卡 onExit 保存当前 HP、下一关 onEnter 恢复；
  // 0 表示从满血开始（新游戏 / 返回开始菜单时由开始场景重置为 0）。
  float playerHealth;
  float runTime; // 全局关卡运行计时（秒），暂停时不计，供关卡 HUD / 速通参考
  // ── 隐式全局计时器（速通，见 systems/speedrun）──────────────────────────
  // 从玩家进入第一关开始计时，失败或通关结束；仅成功通关记录最佳时间并
  // 显示在开始菜单（更优则替换），持久化到 assets/data/save.json。
  float speedrunElapsed; // 当前局速通已计时间（秒）
  bool speedrunActive;   // 当前局是否在速通计时中
  float bestTime;        // 最佳通关时间（秒），< 0 表示尚无记录
  Image icon;            // 窗口图标（保留以便最后卸载）

  // ── 音频总开关（设置界面控制，见 scenes/scene_settings）─────────────────
  // 所有音效/音乐播放统一经 GameAppPlaySound / GameAppPlayMusic 入口，
  // 总开关关闭时静默跳过；持久化到 save.json（见 systems/save_data）。
  bool soundEnabled; // 音效总开关（false 时所有音效静默，默认 true）
  bool musicEnabled; // 音乐总开关（false 时所有音乐静默，默认 true；
                     // 当前版本尚无音乐资源，接口预留）

  Sound uiSound;     // UI 音效（选中/确认，开始/暂停/失败菜单触发播放）
  bool uiSoundValid; // 是否成功加载 UI 音效（无效时静默跳过播放，避免空操作）
  Sound
      meetEnemySound; // 触碰敌怪/进入战斗音效（assets/sounds/meet_the_enemy.ogg）
  bool meetEnemySoundValid; // 是否成功加载（无效时静默跳过播放）

  // ── 关卡/事件音效（assets/sounds/*.ogg）──────────────────────────
  Sound battleWinSound;      // 战斗胜利音效（battle_win.ogg）
  bool battleWinSoundValid;  // 是否成功加载（无效时静默跳过播放）
  Sound catHitSound;         // 玩家受伤音效（cat_hit.ogg）
  bool catHitSoundValid;     // 是否成功加载（无效时静默跳过播放）
  Sound catJumpSound;        // 玩家跳跃音效（cat_jump.ogg）
  bool catJumpSoundValid;    // 是否成功加载（无效时静默跳过播放）
  Sound gameFinishSound;     // 最终通关（通关满 100 关）音效（game_finish.ogg）
  bool gameFinishSoundValid; // 是否成功加载（无效时静默跳过播放）
  Sound gameOverSound;       // 生命值归零失败音效（game_over.ogg）
  bool gameOverSoundValid;   // 是否成功加载（无效时静默跳过播放）
  Sound levelFinishSound;    // 通关单个关卡音效（level_finish.ogg）
  bool levelFinishSoundValid; // 是否成功加载（无效时静默跳过播放）
  Sound pickLetterSound;      // 迷宫关卡拾取字母音效（pick_letter.ogg）
  bool pickLetterSoundValid;  // 是否成功加载（无效时静默跳过播放）
  Sound tickSound;            // 关卡倒计时剩余警告音效（tick.ogg）
  bool tickSoundValid;        // 是否成功加载（无效时静默跳过播放）

  Font uiFont;       // 全局 UI 字体（像素字体，用于界面与中文释义）
  bool uiFontLoaded; // 是否成功加载自定义字体（决定 Close 时是否 UnloadFont）

  // ── 本局错词本/间隔重复抽词（见 systems/study_tracker）──────────────────
  // 由 Run 创建并持有（static），跨关卡共享；新游戏（开始菜单）时重置。
  // 指针字段（不拥有），供各拼写类场景绑定到 Character.study。
  StudyTracker *study;
} GameApp;

// 初始化窗口、图标、音频设备与固定分辨率渲染目标。
// 必须在创建任何场景之前调用（LoadTexture 依赖 InitWindow 完成）。
GameApp GameAppInit(int logicWidth, int logicHeight, const char *title);

// 场景绘制：开始向固定分辨率渲染目标绘制（自动清屏为 RAYWHITE）
void GameAppBegin(GameApp *app);
// 场景绘制：结束渲染目标绘制
void GameAppEnd(GameApp *app);

// 每帧末尾：将渲染结果等比缩放到整个窗口并呈现（保持宽高比居中，多余黑边）
void GameAppPresent(GameApp *app);

// 全局输入：F11 / Alt+Enter 全屏切换（每帧主循环开头调用一次）
void GameAppPollGlobalInput(void);

// 释放渲染目标、图标、音频设备与窗口
void GameAppClose(GameApp *app);

// 全局游戏暂停
void GameAppPaused(GameApp *app);

// 全局游戏继续
void GameAppResume(GameApp *app);

// 使用全局像素字体绘制文本（等价于 DrawText，但应用 uiFont，支持中文释义）。
void GameAppDrawText(const GameApp *app, const char *text, int posX, int posY,
                     int fontSize, Color color);

// 使用全局像素字体测量文本宽度（等价于 MeasureText）。
int GameAppMeasureText(const GameApp *app, const char *text, int fontSize);

// ── 音频总控接口（音效/音乐开关，设置界面经此读写）──────────────────────

// 设置/查询音效总开关（关闭后 GameAppPlaySound 播放被静默跳过）。
void GameAppSetSoundEnabled(GameApp *app, bool enabled);
bool GameAppIsSoundEnabled(const GameApp *app);

// 设置/查询音乐总开关（关闭后 GameAppPlayMusic 播放被静默跳过；
// 当前尚无音乐资源，供后续接入 BGM 使用）。
void GameAppSetMusicEnabled(GameApp *app, bool enabled);
bool GameAppIsMusicEnabled(const GameApp *app);

// 统一音效播放入口：音效总开关关闭或 soundValid 为 false 时静默跳过。
// 全项目播放音效应统一走此接口，避免绕过总开关。
void GameAppPlaySound(const GameApp *app, Sound sound, bool soundValid);

// 统一音乐播放入口（预留）：音乐总开关关闭时静默跳过。
// 接入 BGM 后，加载/播放/循环的调用方在播放前经此接口启动流。
void GameAppPlayMusic(const GameApp *app, Music music);

#endif // GAMEAPP_H
