/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef TOOLS_UI_THEME_H
#define TOOLS_UI_THEME_H

#pragma once
#include <raylib.h>
#include <stdbool.h>

// ─────────────────────────────────────────────────────────────────────────────
// 全局 UI 主题（tools 层）：
//   统一 raygui 控件配色，并提供「带图标 + 键盘选中高亮」的按钮/开关封装，
//   让各菜单（开始 / 暂停 / 失败 / 通关 / 设置）保持一致的视觉风格。
//   - UiThemeApply() 在 GameAppInit 之后调用一次（设置全局样式 + 调用
//     GuiLock 全局锁定 raygui 输入）：此后所有控件彻底不响应鼠标（悬停/
//     点击/tooltip 均失效），UI 交互只由键盘 MenuNav 驱动，绘制不受影响；
//   - UiThemeButton()/UiThemeToggle() 在绘制阶段调用；图标使用 raygui 的
//     "#id#" 图标文本语法，图标由内置 guiIcons 位图绘制，不依赖字体字形。
// ─────────────────────────────────────────────────────────────────────────────

// 应用全局主题配色（覆盖 raygui 默认样式；需在 GameAppInit 之后调用一次）。
void UiThemeApply(void);

// 主题按钮：等价于 GuiButton，可选前置图标（iconId < 0 表示无图标）；
// focused 为真时以 FOCUSED 状态绘制（键盘选中高亮）。
// 返回是否被鼠标点击——UiThemeApply 已 GuiLock，故恒为 false，调用方可忽略
// 返回值，仅把它当作「按键盘选中状态绘制」用。
bool UiThemeButton(Rectangle bounds, int iconId, const char *text,
                   bool focused);

// 主题开关：等价于 GuiToggle（active 为开关状态，由控件回写），可选前置图标。
// 因打开态需要保留绿色配色，键盘选中高亮改用外描边而非 FOCUSED 状态。
// 返回是否发生了切换——同 UiThemeButton，GuiLock 后恒为 false，且不再回写
// active，故调用方应传入「当前真实状态」并忽略返回值（翻转由键盘确认路径完成）。
bool UiThemeToggle(Rectangle bounds, int iconId, const char *text, bool *active,
                   bool focused);

// 主题卡片面板：浅色背景 + 细描边，用作菜单按钮/说明文字的承载卡片。
void UiThemePanel(Rectangle bounds);

#endif // TOOLS_UI_THEME_H
