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

#endif // GAME_CONFIG_H
