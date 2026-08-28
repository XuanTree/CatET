/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SCENE_PLATFORM_H
#define SCENE_PLATFORM_H

#pragma once
#include "core/game_config.h"
#include "core/gameapp.h"
#include "core/gamestack.h"
#include "entities/enemy.h"
#include "entities/flag.h"
#include "entities/platform.h"
#include "entities/player.h"
#include "scenes/scene_transition.h"
#include "systems/speedrun.h"
#include "tools/camera.h"
#include "tools/genrandom.h"
#include "tools/hud.h"
#include "tools/timer.h"
#include <raylib.h>

GameScene *PlatformSceneCreate(const GameApp *app, int difficulty, int level);

#endif //! SCENE_PLATFORM_H