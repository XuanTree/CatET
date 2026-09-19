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

// ── 背景音乐曲目（assets/music/*.mp3，全部循环播放）─────────────────────────
// 每首曲目在 GameAppInit 时统一从内嵌资源流式加载一次，由各场景 onEnter 通过
// GameAppSetMusicTrack 切换；播放/停止统一受 musicEnabled 总开关控制，保证
// 设置界面的「音乐」开关全局生效并持久化。索引顺序不可随意调整（与
// GameAppInit 内的路径表一一对应）。
typedef enum MusicTrack {
  MUSIC_TRACK_MENU = 0, // 主菜单主题曲（CatET.mp3）
  MUSIC_TRACK_PLAY,   // 关卡游玩：平台 / 迷宫 / 极速拼写（Find The Letter.mp3）
  MUSIC_TRACK_BOSS,   // Boss 战（IDK.mp3）
  MUSIC_TRACK_BATTLE, // 战斗场景（Test Your Words.mp3）
  MUSIC_TRACK_INFINITE, // 无尽模式（Wonderful Words Memorizing Time.mp3）
  MUSIC_TRACK_COUNT
} MusicTrack;

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

  // ── 剧情系统（见 systems/story）─────────────────────────────────────────
  // 是否已「完整通关一次游戏」（成功通关第 MAX_LEVELS 关时置 true，并
  // 持久化到 save.json）。为 true 时后续游戏不再显示关卡剧情文本。
  bool isBeatGameOnce;

  Image icon; // 窗口图标（保留以便最后卸载）

  // ── 音频总开关（设置界面控制，见 scenes/scene_settings）─────────────────
  // 音效统一经 GameAppPlaySound、音乐统一经 GameAppSetMusicTrack 入口，
  // 总开关关闭时静默跳过/停止；持久化到 save.json（见 systems/save_data）。
  bool soundEnabled; // 音效总开关（false 时所有音效静默，默认 true）
  bool musicEnabled; // 音乐总开关（false 时所有音乐静默，默认 true；
                     // 设置界面可即时开关并持久化到 save.json）

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

  // ── 背景音乐（BGM）──────────────────────────────────────────────────────
  // 五首曲目统一在 Init 时从内嵌资源流式加载（全部循环播放，raylib 默认），
  // 场景只声明「当前应播放哪首」（GameAppSetMusicTrack），播放/停止由总开关
  // 统一裁决，避免各处零散调用 raylib 音乐 API 绕过设置界面开关。
  Music musicTracks[MUSIC_TRACK_COUNT];
  bool musicTrackValid[MUSIC_TRACK_COUNT]; // 各曲目是否加载成功
  int currentMusicTrack; // 当前曲目索引（MusicTrack）；-1 表示尚未选择/已停止
  bool musicPaused;      // 背景音乐是否因游戏暂停而暂停（仅冻结播放位置，
                         // 恢复后从原位置继续；见 GameAppSetMusicPaused）

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

// 设置/查询音乐总开关（关闭后立即停止当前 BGM，重新开启时自动恢复播放；
// 设置界面经此读写并持久化）。
void GameAppSetMusicEnabled(GameApp *app, bool enabled);
bool GameAppIsMusicEnabled(const GameApp *app);

// 统一音效播放入口：音效总开关关闭或 soundValid 为 false 时静默跳过。
// 全项目播放音效应统一走此接口，避免绕过总开关。
void GameAppPlaySound(const GameApp *app, Sound sound, bool soundValid);

// ── 背景音乐统一接口 ────────────────────────────────────────────────────────
// 切换背景音乐曲目：与当前曲目相同则忽略；切换时自动停止上一首，并依据音乐
// 总开关决定是否立即播放新曲目（关闭时仅记住曲目，重新开启后自动续播）。
// 各场景在 onEnter（覆盖层则在 onResume）调用，实现全局统一 BGM 调度。
void GameAppSetMusicTrack(GameApp *app, MusicTrack track);

// 停止当前背景音乐并清除当前曲目（一般无需手动调用，供特殊场景静音使用）。
void GameAppStopMusic(GameApp *app);

// 暂停/恢复当前背景音乐：暂停仅冻结播放位置，恢复后从原位置继续（不会
// 重新开始播放）。主循环随游戏暂停状态每帧同步（见 Run）；再次切换曲目时
// 若处于暂停态，只记忆曲目而不播放，待恢复后自动续播。
void GameAppSetMusicPaused(GameApp *app, bool paused);

// 每帧驱动：更新当前曲目的流缓冲（主循环每帧调用一次，与帧率无关）。
void GameAppUpdateMusic(const GameApp *app);

#endif // GAMEAPP_H
