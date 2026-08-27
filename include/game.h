#ifndef GAME_H
#define GAME_H

#pragma once

#include <limits.h> // INT_MAX
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h> // SIZE_MAX
#include <stdio.h>
#include <stdlib.h> // malloc / realloc / free
#include <string.h>
#include <time.h>

#include <raylib.h>
#include <raymath.h>

// ─── core：框架层（仅依赖 raylib）────────────────────────────────────────
#include "core/game_config.h"
#include "core/gameapp.h"
#include "core/gamestack.h"

// ─── tools：工具层（hud 依赖 core/gameapp）───────────────────────────────
#include "extern/raygui.h"
#include "tools/animation.h"
#include "tools/camera.h"
#include "tools/genrandom.h"
#include "tools/hud.h"
#include "tools/menu.h"
#include "tools/strings.h"
#include "tools/timer.h"


// ─── systems：跨场景系统层（仅依赖 core）─────────────────────────────────
#include "systems/dialogue.h"
#include "systems/level_flow.h"
#include "systems/save_data.h"
#include "systems/speedrun.h"
#include "systems/words_loader.h"

// ─── entities：实体层（依赖 tools / systems；组内顺序无关）───────────────
#include "entities/boss.h"
#include "entities/bullet.h"
#include "entities/character.h"
#include "entities/enemy.h"
#include "entities/flag.h"
#include "entities/maze.h"
#include "entities/platform.h"
#include "entities/player.h"

// ─── scenes：场景层（依赖以上全部）───────────────────────────────────────
#include "scenes/scene_battle.h"
#include "scenes/scene_fail.h"
#include "scenes/scene_maze.h"
#include "scenes/scene_pause.h"
#include "scenes/scene_platform.h"
#include "scenes/scene_start.h"
#include "scenes/scene_test.h"
#include "scenes/scene_transition.h"

void Run();

#endif // GAME_H
