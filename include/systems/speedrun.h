#ifndef SYSTEMS_SPEEDRUN_H
#define SYSTEMS_SPEEDRUN_H

#pragma once
#include "core/gameapp.h"

// ─────────────────────────────────────────────────────────────────────────────
// 隐式全局计时器（速通，系统层）：
//   - 玩家从进入第一关开始计时（SpeedrunStart），期间跨关卡保持累计；
//   - 失败（SpeedrunStop）或成功通关（SpeedrunFinish）时停止计时；
//   - 仅成功通关记录最佳时间并持久化（save_data），开始菜单显示，
//     下次更优的数据则替换。
//   状态字段（speedrunElapsed / speedrunActive / bestTime）存于 GameApp，
//   本模块只负责操作这些字段并组合 save_data 做读写。
// ─────────────────────────────────────────────────────────────────────────────

// 启动游戏时调用：读取已持久化的最佳时间（无记录置 -1），清空速通状态。
void SpeedrunInit(GameApp *app);

// 进入第一关：清零并开始速通计时（重复调用安全，总是从 0 重新计时）。
void SpeedrunStart(GameApp *app);

// 失败/离开游戏进程：停止计时，不记录任何数据。
void SpeedrunStop(GameApp *app);

// 主循环每帧调用：仅当在计时中累计 elapsed。
void SpeedrunTick(GameApp *app, float dt);

// 成功通关（最终胜利）：停止计时；若当前耗时优于最佳则更新并持久化。
// 返回是否刷新了最佳记录（用于开始菜单提示「新纪录」）。
bool SpeedrunFinish(GameApp *app);

#endif // SYSTEMS_SPEEDRUN_H
