#ifndef SYSTEMS_SAVE_DATA_H
#define SYSTEMS_SAVE_DATA_H

#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// 最佳通关时间持久化（assets/data/save.json）：
//   游戏采用肉鸽形式不设存档读档，仅持久化「玩家成功通关所花时间」的最佳记录，
//   每次启动游戏时读取并显示在开始界面（docs/game_instructions.md
//   游戏数据持久化）。
// ─────────────────────────────────────────────────────────────────────────────

// 读取最佳通关时间（秒）；文件缺失或格式损坏返回 -1（表示尚无记录）。
float SaveDataLoadBestTime(void);

// 写入最佳通关时间（秒）到 save.json；bestTime < 0 时忽略。
void SaveDataSaveBestTime(float bestTime);

#endif // SYSTEMS_SAVE_DATA_H
