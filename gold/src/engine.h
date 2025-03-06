#pragma once

#include "defines.h"
#include "util.h"
#include "engine_types.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <vector>
#include <unordered_map>

#define RENDER_TEXT_CENTERED INT32_MAX
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

struct player_color_t {
    const char* name;
    SDL_Color skin_color;
    SDL_Color clothes_color;
};

extern const player_color_t PLAYER_COLORS[MAX_PLAYERS];
const SDL_Color RECOLOR_SKIN_REF = (SDL_Color) { .r = 123, .g = 174, .b = 121, .a = 255 };
const SDL_Color RECOLOR_CLOTHES_REF = (SDL_Color) { .r = 255, .g = 0, .b = 255, .a = 255 };

// FONT

const uint32_t FONT_GLYPH_COUNT = 95;

struct font_glyph_t {
    SDL_Texture* texture;
    int width;
    int height;
    int bearing_x;
    int bearing_y;
    int advance;
};

struct font_t {
    font_glyph_t glyphs[FONT_GLYPH_COUNT];
};

// SPRITE

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

// Options

enum Option {
    OPTION_DISPLAY,
    OPTION_VSYNC,
    OPTION_SFX_VOLUME,
    OPTION_MUS_VOLUME,
    OPTION_UNIT_VOICES,
    OPTION_CAMERA_SPEED,
    OPTION_COUNT
};

enum OptionValueDisplay {
    DISPLAY_WINDOWED,
    DISPLAY_FULLSCREEN,
    DISPLAY_BORDERLESS,
    DISPLAY_COUNT
};

enum OptionValueVsync {
    VSYNC_DISABLED,
    VSYNC_ENABLED,
    VSYNC_COUNT
};

enum OptionValueVoices {
    VOICES_ON,
    VOICES_OFF,
    VOICES_COUNT
};

enum OptionType {
    OPTION_TYPE_DROPDOWN,
    OPTION_TYPE_SLIDER_PERCENT,
    OPTION_TYPE_SLIDER
};

struct option_data_t {
    const char* name;
    OptionType type;
    int min_value;
    int max_value;
    int default_value;
};
extern std::unordered_map<Option, option_data_t> OPTION_DATA;

struct engine_t {
    SDL_Window* window;
    SDL_Renderer* renderer;
    const uint8_t* keystate;

    std::vector<font_t> fonts;
    std::vector<sprite_t> sprites;
    std::vector<uint16_t> tile_index;
    std::unordered_map<uint32_t, uint32_t> neighbors_to_autotile_index;
    std::vector<SDL_Cursor*> cursors;
    SDL_Texture* minimap_texture = NULL;
    SDL_Texture* minimap_tiles_texture = NULL;
    std::vector<Mix_Chunk*> sounds;
    std::vector<uint32_t> sound_index;

    xy mouse_position;
    Cursor current_cursor;
    bool render_debug_info;
    std::unordered_map<Option, int> options;
    std::unordered_map<UiButton, SDL_Keycode> hotkeys;
};
extern engine_t engine;

bool engine_init();
bool engine_load_textures();
void engine_free_textures();
void engine_quit();

void engine_set_cursor(Cursor cursor);

void engine_apply_option(Option option, int value);
void engine_load_options();
void engine_save_options();
int engine_sdl_key_str(char* str_ptr, SDL_Keycode hotkey);

extern const std::unordered_map<UiButton, SDL_Keycode> DEFAULT_HOTKEYS;
extern const std::unordered_map<UiButton, SDL_Keycode> GRID_HOTKEYS;

enum TextAnchor {
    TEXT_ANCHOR_TOP_LEFT,
    TEXT_ANCHOR_BOTTOM_LEFT,
    TEXT_ANCHOR_TOP_RIGHT,
    TEXT_ANCHOR_BOTTOM_RIGHT,
    TEXT_ANCHOR_TOP_CENTER
};

xy autotile_edge_lookup(uint32_t edge, uint8_t neighbors);
xy render_get_text_size(Font font, const char* text);
void render_text(Font font, const char* text, xy position, TextAnchor anchor = TEXT_ANCHOR_TOP_LEFT);

const uint32_t RENDER_SPRITE_FLIP_H = 1;
const uint32_t RENDER_SPRITE_CENTERED = 2;
const uint32_t RENDER_SPRITE_NO_CULL = 4;

void ysort_render_params(std::vector<render_sprite_params_t>& params, int low, int high);
void render_sprite(Sprite sprite, const xy& frame, const xy& position, uint32_t options = 0, uint8_t recolor_id = RECOLOR_NONE);
void render_ninepatch(Sprite sprite, const SDL_Rect& rect, int patch_margin);

void render_text_with_text_frame(Sprite sprite, Font font, const char* text, xy position, xy text_offset, bool center_text = true);
SDL_Rect get_text_with_text_frame_rect(Sprite sprite, Font font, const char* text, xy position);
void render_menu_button(const char* text, xy position, bool hovered);
SDL_Rect render_get_button_rect(const char* text, xy position);

void sound_play(Sound sound);