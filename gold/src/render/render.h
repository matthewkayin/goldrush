#pragma once

#include "math/gmath.h"
#include "resource/sprite.h"
#include "resource/font.h"
#include <SDL2/SDL.h>

struct color_t {
    float r;
    float g;
    float b;
    float a;
};

enum recolor_t: uint8_t {
    RECOLOR_NONE
};

enum minimap_layer {
    MINIMAP_LAYER_TILE,
    MINIMAP_LAYER_FOG
};

enum minimap_pixel {
    MINIMAP_PIXEL_TRANSPARENT,
    MINIMAP_PIXEL_OFFBLACK,
    MINIMAP_PIXEL_OFFBLACK_TRANSPARENT,
    MINIMAP_PIXEL_WHITE,
    MINIMAP_PIXEL_PLAYER0,
    MINIMAP_PIXEL_PLAYER1,
    MINIMAP_PIXEL_PLAYER2,
    MINIMAP_PIXEL_PLAYER3,
    MINIMAP_PIXEL_COUNT
};

const uint32_t RENDER_SPRITE_NO_CULL = 1;
const uint32_t RENDER_SPRITE_FLIP_H = 2;
const uint32_t RENDER_SPRITE_CENTERED = 4;

bool render_init(SDL_Window* window);
void render_quit();
void render_prepare_frame();
void render_present_frame();
void render_sprite(sprite_name name, ivec2 frame, ivec2 position, int z_index, uint32_t options, int recolor_id);
void render_ninepatch(sprite_name sprite, rect_t rect, int z_index);
void render_atlas(atlas_name atlas, rect_t src_rect, rect_t dst_rect, int z_index, uint32_t options);
void render_text(font_name name, const char* text, ivec2 position, int z_index, color_t color);
void render_line(ivec2 start, ivec2 end, int z_index, color_t color);
void render_rect(ivec2 start, ivec2 end, int z_index, color_t color);
void render_minimap_putpixel(minimap_layer layer, ivec2 position, minimap_pixel pixel);
void render_minimap(ivec2 position, int z_index, ivec2 src_size, ivec2 dst_size);