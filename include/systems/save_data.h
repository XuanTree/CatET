/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SYSTEMS_SAVE_DATA_H
#define SYSTEMS_SAVE_DATA_H

#pragma once
#include <stdbool.h>

// ─────────────────────────────────────────────────────────────────────────────
// 最佳通关时间 + 音频设置持久化（assets/data/save.json）：
//   游戏采用肉鸽形式不设存档读档，仅持久化「玩家成功通关所花时间」的最佳记录
//   与「音效/音乐总开关」（设置界面修改），每次启动游戏时读取并生效
//   （docs/game_instructions.md 游戏数据持久化）。
//   文件格式：
//     {"bestTime": 123.45, "infiniteBest": 20, "soundEnabled": true,
//      "musicEnabled": true}
//   全部字段须在下方 SaveData 结构登记，读写统一走 LoadAll/SaveAll，
//   避免多个写入口互相覆盖。
// ─────────────────────────────────────────────────────────────────────────────

// 读取最佳通关时间（秒）；文件缺失或格式损坏返回 -1（表示尚无记录）。
float SaveDataLoadBestTime(void);

// 写入最佳通关时间（秒）到 save.json；bestTime < 0 时忽略。
// 保留其余字段（无尽纪录/音频设置）不被覆盖。
void SaveDataSaveBestTime(float bestTime);

// ── 无尽模式最佳纪录持久化（独立于主线速通，单独字段 + 单独读写入口）────
// 无尽模式只纪录「单局最高答对数」，与 bestTime 语义完全无关：不进速通计时、
// 不被主线通关/失败逻辑触碰，仅由 scene_infinite 自己读写。

// 读取无尽模式最高答对数；文件缺失/无纪录返回 0。
int SaveDataLoadInfiniteBest(void);

// 若 best 严格优于已存纪录则更新并写回（保留 bestTime / 音频设置），
// 返回是否刷新了纪录；best <= 0 时忽略返回 false。
bool SaveDataSaveInfiniteBest(int best);

// ── 音频设置持久化（音效/音乐总开关）──────────────────────────────────

// 读取音效总开关；文件缺失、字段缺失或格式损坏返回 DEFAULT_SOUND_ENABLED。
bool SaveDataLoadSoundEnabled(void);

// 读取音乐总开关；文件缺失、字段缺失或格式损坏返回 DEFAULT_MUSIC_ENABLED。
bool SaveDataLoadMusicEnabled(void);

// 写入音效/音乐总开关到 save.json（保留 bestTime 字段不被覆盖）。
void SaveDataSaveSettings(bool soundEnabled, bool musicEnabled);

#endif // SYSTEMS_SAVE_DATA_H
