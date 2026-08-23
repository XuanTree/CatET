#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

// 全局统一缩放：玩家与平台共用同一缩放，保证绘制与碰撞处于相同比例，
// 避免不同物体缩放不一致导致“悬浮 / 贴图不吻合”的观感。
#define GAME_SCALE 3.0f
#define TRANSITION_SECONDS 0.45f

#endif // GAME_CONFIG_H
