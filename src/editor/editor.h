#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include <SDL3/SDL.h>

void editor_init(SDL_Window* window);
void editor_quit();
void editor_update();
void editor_render();

#endif