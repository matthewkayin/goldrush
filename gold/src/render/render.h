#pragma once

#include "math/gmath.h"
#include <SDL2/SDL.h>

enum Recolor: uint8_t {
    RECOLOR_NONE
};

const uint32_t RENDER_SPRITE_NO_CULL = 1;
const uint32_t RENDER_SPRITE_FLIP_H = 2;
const uint32_t RENDER_SPRITE_CENTERED = 4;

bool render_init(SDL_Window* window);
void render_quit();
void render_prepare_frame();
void render_present_frame();
void render_sprite(ivec2 frame, ivec2 position, int z_index, uint32_t options = 0, uint8_t recolor = RECOLOR_NONE);