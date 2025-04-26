#pragma once

#include "math/gmath.h"
#include "render/sprite.h"
#include "render/font.h"
#include <SDL3/SDL.h>

#define AUTOTILE_HFRAMES 8
#define AUTOTILE_VFRAMES 6

enum RenderDisplay {
    RENDER_DISPLAY_WINDOWED,
    RENDER_DISPLAY_FULLSCREEN,
    RENDER_DISPLAY_COUNT
};

enum RenderVsync {
    RENDER_VSYNC_DISABLED,
    RENDER_VSYNC_ENABLED,
    RENDER_VSYNC_ADAPTIVE,
    RENDER_VSYNC_COUNT
};

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
    MINIMAP_PIXEL_GOLD,
    MINIMAP_PIXEL_SAND,
    MINIMAP_PIXEL_WATER,
    MINIMAP_PIXEL_WALL,
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

enum RenderColor {
    RENDER_COLOR_WHITE,
    RENDER_COLOR_OFFBLACK,
    RENDER_COLOR_OFFBLACK_TRANSPARENT,
    RENDER_COLOR_DARK_GRAY,
    RENDER_COLOR_BLUE,
    RENDER_COLOR_DIM_BLUE,
    RENDER_COLOR_LIGHT_BLUE,
    RENDER_COLOR_RED,
    RENDER_COLOR_RED_TRANSPARENT,
    RENDER_COLOR_LIGHT_RED,
    RENDER_COLOR_GOLD,
    RENDER_COLOR_DIM_SAND,
    RENDER_COLOR_GREEN,
    RENDER_COLOR_GREEN_TRANSPARENT,
    RENDER_COLOR_DARK_GREEN,
    RENDER_COLOR_PURPLE,
    RENDER_COLOR_LIGHT_PURPLE,
    RENDER_COLOR_COUNT
};

const RenderColor RENDER_PLAYER_COLORS[MAX_PLAYERS] = { 
    RENDER_COLOR_BLUE, 
    RENDER_COLOR_RED, 
    RENDER_COLOR_DARK_GREEN, 
    RENDER_COLOR_PURPLE 
};
const RenderColor RENDER_PLAYER_SKIN_COLORS[MAX_PLAYERS] = {
    RENDER_COLOR_LIGHT_BLUE,
    RENDER_COLOR_LIGHT_RED,
    RENDER_COLOR_GREEN,
    RENDER_COLOR_LIGHT_PURPLE
};

const uint32_t RENDER_SPRITE_NO_CULL = 1;
const uint32_t RENDER_SPRITE_FLIP_H = 2;
const uint32_t RENDER_SPRITE_CENTERED = 4;

bool render_init(SDL_Window* window);
void render_quit();
void render_update_screen_scale();
void render_set_display(RenderDisplay display);
void render_set_vsync(RenderVsync vsync);
void render_request_report();
void render_sprite_batch();
void render_prepare_frame();
void render_present_frame();
const SpriteInfo& render_get_sprite_info(SpriteName name);
void render_sprite_frame(SpriteName name, ivec2 frame, ivec2 position, uint32_t options, int recolor_id);
void render_ninepatch(SpriteName sprite, Rect rect);
void render_sprite(SpriteName sprite, Rect src_rect, Rect dst_rect, uint32_t options);
void render_text(FontName name, const char* text, ivec2 position);
ivec2 render_get_text_size(FontName name, const char* text);
void render_line(ivec2 start, ivec2 end, RenderColor color);
void render_draw_rect(Rect rect, RenderColor color);
void render_fill_rect(Rect rect, RenderColor color);
void render_minimap_putpixel(MinimapLayer layer, ivec2 position, MinimapPixel pixel);
void render_minimap_draw_rect(MinimapLayer layer, Rect rect, MinimapPixel pixel);
void render_minimap_fill_rect(MinimapLayer layer, Rect rect, MinimapPixel pixel);
void render_minimap(ivec2 position, ivec2 src_size, ivec2 dst_size);