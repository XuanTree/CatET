#ifndef SCENE_FAIL_H
#define SCENE_FAIL_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"

// 创建失败场景：当玩家 HP ≤ 0 时由关卡场景 Replace 进入，显示失败画面。
// 玩家只能选择「回到菜单」（返回开始界面）或「退出游戏」两项。
GameScene *FailSceneCreate(const GameApp *app);

#endif // !SCENE_FAIL_H
