#ifndef SCENE_TEST_H
#define SCENE_TEST_H

#pragma once
#include "core/gameapp.h"
#include "core/gamestack.h"

// 创建测试场景：创建人物的单关卡玩法，行为保持一致，仅用于关卡测试
// 作为骨架的验证载体，后续可据此拆分为 MenuScene / LevelScene 等。
GameScene *TestSceneCreate(const GameApp *app);

#endif // SCENE_TEST_H
