#ifndef SCENE_BATTLE_H
#define SCENE_BATTLE_H

#pragma once
#include "core/game_config.h"
#include "core/gameapp.h"
#include "core/gamestack.h"
#include "entities/character.h"
#include "entities/enemy.h"
#include "entities/player.h"
#include "systems/speedrun.h"
#include "tools/animation.h"
#include "tools/camera.h"
#include "tools/genrandom.h"
#include "tools/timer.h"
#include <raylib.h>

GameScene *BattleSceneCreate(const GameApp *app, int difficulty);

#endif //! SCENE_BATTLE_H