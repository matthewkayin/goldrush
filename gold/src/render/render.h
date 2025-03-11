#pragma once

#include <SDL2/SDL.h>

bool render_init(SDL_Window* window);
void render_quit();
void render_prepare_frame();
void render_present_frame();