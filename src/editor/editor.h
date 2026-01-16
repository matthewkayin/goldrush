#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include <SDL3/SDL.h>
#include "scenario/scenario.h"

void editor_init(SDL_Window* window);
void editor_quit();
void editor_update();
void editor_render();

bool editor_requests_playtest();
void editor_begin_playtest();
void editor_end_playtest();
const Scenario* editor_get_scenario();

#endif