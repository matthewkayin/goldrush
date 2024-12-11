#pragma once

#include "defines.h"
#include "util.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <vector>
#include <unordered_map>

#define RENDER_TEXT_CENTERED -1
#define RECOLOR_NONE UINT8_MAX

// COLORS

const SDL_Color COLOR_BLACK = (SDL_Color) { .r = 0, .g = 0, .b = 0, .a = 255 };
const SDL_Color COLOR_OFFBLACK = (SDL_Color) { .r = 40, .g = 37, .b = 45, .a = 255 };
const SDL_Color COLOR_DARKBLACK = (SDL_Color) { .r = 16, .g = 15, .b = 18, .a = 255 };
const SDL_Color COLOR_WHITE = (SDL_Color) { .r = 255, .g = 255, .b = 255, .a = 255 };
const SDL_Color COLOR_SKY_BLUE = (SDL_Color) { .r = 92, .g = 132, .b = 153, .a = 255 };
const SDL_Color COLOR_DIM_BLUE = (SDL_Color) { .r = 70, .g = 100, .b = 115, .a = 255 };
const SDL_Color COLOR_SAND_DARK = (SDL_Color) { .r = 204, .g = 162, .b = 139, .a = 255 };
const SDL_Color COLOR_RED = (SDL_Color) { .r = 186, .g = 97, .b = 95, .a = 255 };
const SDL_Color COLOR_GREEN = (SDL_Color) { .r = 123, .g = 174, .b = 121, .a = 255 };
const SDL_Color COLOR_GOLD = (SDL_Color) { .r = 238, .g = 209, .b = 158, .a = 255 };
const SDL_Color COLOR_DARK_GRAY = (SDL_Color) { .r = 94, .g = 88, .b = 89, .a = 255 };

const SDL_Color PLAYER_COLORS[MAX_PLAYERS] = {
    (SDL_Color) { .r = 92, .g = 132, .b = 153, .a = 255 }, // Blue
    (SDL_Color) { .r = 181, .g = 94, .b = 94, .a = 255 }, // Red
    (SDL_Color) { .r = 144, .g = 119, .b = 153, .a = 255 }, // Purple
    (SDL_Color) { .r = 212, .g = 166, .b = 80, .a = 255 } // Yellow
};
const SDL_Color RECOLOR_REF = (SDL_Color) { .r = 255, .g = 0, .b = 255, .a = 255 };

// FONT

enum Font {
    FONT_HACK,
    FONT_WESTERN8,
    FONT_WESTERN16,
    FONT_WESTERN32,
    FONT_M3X6,
    FONT_COUNT
};

// SPRITE

enum UiButton {
    UI_BUTTON_NONE,
    UI_BUTTON_MOVE,
    UI_BUTTON_STOP,
    UI_BUTTON_ATTACK,
    UI_BUTTON_DEFEND,
    UI_BUTTON_BUILD,
    UI_BUTTON_REPAIR,
    UI_BUTTON_CANCEL,
    UI_BUTTON_UNLOAD,
    UI_BUTTON_BUILD_HOUSE,
    UI_BUTTON_BUILD_CAMP,
    UI_BUTTON_BUILD_SALOON,
    UI_BUTTON_BUILD_BUNKER,
    UI_BUTTON_MINE,
    UI_BUTTON_UNIT_MINER,
    UI_BUTTON_UNIT_COWBOY,
    UI_BUTTON_UNIT_WAGON,
    UI_BUTTON_UNIT_BANDIT,
    UI_BUTTON_COUNT
};

enum Sprite {
    SPRITE_TILESET_ARIZONA,
    SPRITE_TILE_DECORATION,
    SPRITE_TILE_GOLD,
    SPRITE_UI_FRAME,
    SPRITE_UI_FRAME_BOTTOM,
    SPRITE_UI_FRAME_BUTTONS,
    SPRITE_UI_MINIMAP,
    SPRITE_UI_BUTTON,
    SPRITE_UI_BUTTON_ICON,
    SPRITE_UI_TOOLTIP_FRAME,
    SPRITE_UI_TEXT_FRAME,
    SPRITE_UI_TABS,
    SPRITE_UI_GOLD,
    SPRITE_UI_HOUSE,
    SPRITE_UI_MOVE,
    SPRITE_UI_PARCHMENT_BUTTONS,
    SPRITE_UI_OPTIONS_DROPDOWN,
    SPRITE_UI_CONTROL_GROUP_FRAME,
    SPRITE_UI_MENU_BUTTON,
    SPRITE_UI_MENU_PARCHMENT_BUTTONS,
    SPRITE_SELECT_RING_UNIT_1,
    SPRITE_SELECT_RING_UNIT_1_ENEMY,
    SPRITE_SELECT_RING_UNIT_2,
    SPRITE_SELECT_RING_UNIT_2_ENEMY,
    SPRITE_SELECT_RING_BUILDING_2,
    SPRITE_SELECT_RING_BUILDING_2_ENEMY,
    SPRITE_SELECT_RING_BUILDING_3,
    SPRITE_SELECT_RING_BUILDING_3_ENEMY,
    SPRITE_MINER_BUILDING,
    SPRITE_UNIT_MINER,
    SPRITE_UNIT_COWBOY,
    SPRITE_UNIT_WAGON,
    SPRITE_UNIT_BANDIT,
    SPRITE_BUILDING_HOUSE,
    SPRITE_BUILDING_CAMP,
    SPRITE_BUILDING_SALOON,
    SPRITE_BUILDING_BUNKER,
    SPRITE_BUILDING_DESTROYED_2,
    SPRITE_BUILDING_DESTROYED_3,
    SPRITE_BUILDING_DESTROYED_BUNKER,
    SPRITE_FOG_OF_WAR,
    SPRITE_RALLY_FLAG,
    SPRITE_MENU_CLOUDS,
    SPRITE_MENU_REFRESH,
    SPRITE_PARTICLE_SPARKS,
    SPRITE_PARTICLE_BUNKER_COWBOY,
    SPRITE_COUNT
};

enum Tile {
    TILE_NULL,
    TILE_SAND,
    TILE_SAND2,
    TILE_SAND3,
    TILE_WATER,
    TILE_WALL_NW_CORNER,
    TILE_WALL_NE_CORNER,
    TILE_WALL_SW_CORNER,
    TILE_WALL_SE_CORNER,
    TILE_WALL_NORTH_EDGE,
    TILE_WALL_WEST_EDGE,
    TILE_WALL_EAST_EDGE,
    TILE_WALL_SOUTH_EDGE,
    TILE_WALL_SE_FRONT,
    TILE_WALL_SOUTH_FRONT,
    TILE_WALL_SW_FRONT,
    TILE_WALL_NW_INNER_CORNER,
    TILE_WALL_NE_INNER_CORNER,
    TILE_WALL_SW_INNER_CORNER,
    TILE_WALL_SE_INNER_CORNER,
    TILE_WALL_SOUTH_STAIR_LEFT,
    TILE_WALL_SOUTH_STAIR_CENTER,
    TILE_WALL_SOUTH_STAIR_RIGHT,
    TILE_WALL_SOUTH_STAIR_FRONT_LEFT,
    TILE_WALL_SOUTH_STAIR_FRONT_CENTER,
    TILE_WALL_SOUTH_STAIR_FRONT_RIGHT,
    TILE_WALL_NORTH_STAIR_LEFT,
    TILE_WALL_NORTH_STAIR_CENTER,
    TILE_WALL_NORTH_STAIR_RIGHT,
    TILE_WALL_EAST_STAIR_TOP,
    TILE_WALL_EAST_STAIR_CENTER,
    TILE_WALL_EAST_STAIR_BOTTOM,
    TILE_WALL_WEST_STAIR_TOP,
    TILE_WALL_WEST_STAIR_CENTER,
    TILE_WALL_WEST_STAIR_BOTTOM,
    TILE_COUNT
};

struct sprite_t {
    union {
        SDL_Texture* texture;
        SDL_Texture* colored_texture[MAX_PLAYERS];
    };
    xy frame_size;
    int hframes;
    int vframes;
};

const SDL_Rect SCREEN_RECT = (SDL_Rect) {
    .x = 0, .y = 0, 
    .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT
};

enum Cursor {
    CURSOR_DEFAULT,
    CURSOR_TARGET,
    CURSOR_COUNT
};

struct engine_t {
    SDL_Window* window;
    SDL_Renderer* renderer;

    std::vector<TTF_Font*> fonts;
    std::vector<sprite_t> sprites;
    std::vector<uint16_t> tile_index;
    std::vector<SDL_Cursor*> cursors;

    xy mouse_position;
    Cursor current_cursor;
};
extern engine_t engine;

bool engine_init();
bool engine_init_renderer();
void engine_destroy_renderer();
void engine_quit();

void engine_set_cursor(Cursor cursor);

enum TextAnchor {
    TEXT_ANCHOR_TOP_LEFT,
    TEXT_ANCHOR_BOTTOM_LEFT,
    TEXT_ANCHOR_TOP_RIGHT,
    TEXT_ANCHOR_BOTTOM_RIGHT,
    TEXT_ANCHOR_TOP_CENTER
};

xy autotile_edge_lookup(uint32_t edge, uint8_t neighbors);
void render_text(Font font, const char* text, SDL_Color color, xy position, TextAnchor anchor = TEXT_ANCHOR_TOP_LEFT);

struct render_sprite_params_t {
    Sprite sprite;
    xy frame;
    xy position;
    uint32_t options;
    uint8_t recolor_id;
};
const uint32_t RENDER_SPRITE_FLIP_H = 1;
const uint32_t RENDER_SPRITE_CENTERED = 2;
const uint32_t RENDER_SPRITE_NO_CULL = 4;

void ysort_render_params(std::vector<render_sprite_params_t>& params, int low, int high);
void render_sprite(Sprite sprite, const xy& frame, const xy& position, uint32_t options = 0, uint8_t recolor_id = RECOLOR_NONE);
void render_ninepatch(Sprite sprite, const SDL_Rect& rect, int patch_margin);