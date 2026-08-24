#ifndef GAMEAPP_H
#define GAMEAPP_H

#pragma once
#include <math.h>
#include <raylib.h>

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
  Image icon;    // 窗口图标（保留以便最后卸载）
  Sound uiSound; // UI 音效（选中/确认，开始/暂停/失败菜单触发播放）
  bool uiSoundValid; // 是否成功加载 UI 音效（无效时静默跳过播放，避免空操作）
  Font uiFont;       // 全局 UI 字体（像素字体，用于界面与中文释义）
  bool uiFontLoaded; // 是否成功加载自定义字体（决定 Close 时是否 UnloadFont）
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

#endif // GAMEAPP_H
