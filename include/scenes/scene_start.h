#ifndef SCENE_START_H
#define SCENE_START_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"
#include "tools/raygui.h"
#include <raylib.h>

// 创建开始菜单场景：标题 + 开始/设置/退出按钮。
// 点击 Play 切换至测试关卡场景，点击 Quit 退出游戏。
GameScene *StartSceneCreate(GameApp *app);

#endif // !SCENE_START_H
