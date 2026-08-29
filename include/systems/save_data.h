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

// ─────────────────────────────────────────────────────────────────────────────
// 最佳通关时间 + 音频设置持久化（assets/data/save.json）：
//   游戏采用肉鸽形式不设存档读档，仅持久化「玩家成功通关所花时间」的最佳记录
//   与「音效/音乐总开关」（设置界面修改），每次启动游戏时读取并生效
//   （docs/game_instructions.md 游戏数据持久化）。
//   文件格式：{"bestTime": 123.45, "soundEnabled": true, "musicEnabled": true}
// ─────────────────────────────────────────────────────────────────────────────

// 读取最佳通关时间（秒）；文件缺失或格式损坏返回 -1（表示尚无记录）。
float SaveDataLoadBestTime(void);

// 写入最佳通关时间（秒）到 save.json；bestTime < 0 时忽略。
// 保留其余字段（音频设置）不被覆盖。
void SaveDataSaveBestTime(float bestTime);

// ── 音频设置持久化（音效/音乐总开关）──────────────────────────────────

// 读取音效总开关；文件缺失、字段缺失或格式损坏返回 DEFAULT_SOUND_ENABLED。
bool SaveDataLoadSoundEnabled(void);

// 读取音乐总开关；文件缺失、字段缺失或格式损坏返回 DEFAULT_MUSIC_ENABLED。
bool SaveDataLoadMusicEnabled(void);

// 写入音效/音乐总开关到 save.json（保留 bestTime 字段不被覆盖）。
void SaveDataSaveSettings(bool soundEnabled, bool musicEnabled);

#endif // SYSTEMS_SAVE_DATA_H
