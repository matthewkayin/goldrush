#pragma once

#include "math/gmath.h"
#include "render/sprite.h"
#include "render/font.h"
#include <SDL2/SDL.h>

#define AUTOTILE_HFRAMES 8
#define AUTOTILE_VFRAMES 6

enum MinimapLayer {
    MINIMAP_LAYER_TILE,
    MINIMAP_LAYER_FOG
};

enum MinimapPixel {
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

struct SpriteInfo {
    int atlas;
    int atlas_x;
    int atlas_y;
    int hframes;
    int vframes;
    int frame_width;
    int frame_height;
};

const uint32_t RENDER_SPRITE_NO_CULL = 1;
const uint32_t RENDER_SPRITE_FLIP_H = 2;
const uint32_t RENDER_SPRITE_CENTERED = 4;

bool render_init(SDL_Window* window);
void render_quit();
void render_set_window_size(ivec2 window_size);
void render_prepare_frame();
void render_present_frame();
const SpriteInfo& render_get_sprite_info(SpriteName name);
void render_sprite_frame(SpriteName name, ivec2 frame, ivec2 position, uint32_t options, int recolor_id);
void render_ninepatch(SpriteName sprite, Rect rect);
void render_sprite(SpriteName sprite, Rect src_rect, Rect dst_rect, uint32_t options);
void render_text(FontName name, const char* text, ivec2 position);
ivec2 render_get_text_size(FontName name, const char* text);
void render_line(ivec2 start, ivec2 end);
void render_rect(ivec2 start, ivec2 end);
void render_minimap_putpixel(MinimapLayer layer, ivec2 position, MinimapPixel pixel);
void render_minimap(ivec2 position, ivec2 src_size, ivec2 dst_size);