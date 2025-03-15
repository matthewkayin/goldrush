#pragma once

#include "math/gmath.h"
#include "resource/sprite.h"
#include "resource/font.h"
#include <SDL2/SDL.h>

struct color_t {
    float r;
    float g;
    float b;
};

enum recolor_t: uint8_t {
    RECOLOR_NONE
};

const uint32_t RENDER_SPRITE_NO_CULL = 1;
const uint32_t RENDER_SPRITE_FLIP_H = 2;
const uint32_t RENDER_SPRITE_CENTERED = 4;

bool render_init(SDL_Window* window);
void render_quit();
void render_prepare_frame();
void render_present_frame();
void render_sprite(sprite_name name, rect_t src_rect, rect_t dst_rect, int z_index, uint32_t options);
void render_sprite_frame(sprite_name name, ivec2 frame, ivec2 position, int z_index, uint32_t options, int recolor_id);
void render_text(font_name name, const char* text, ivec2 position, int z_index, color_t color);
void render_line(ivec2 start, ivec2 end, int z_index, color_t color);
void render_rect(ivec2 start, ivec2 end, int z_index, color_t color);
void render_fill_rect(ivec2 start, ivec2 end, int z_index, color_t color);