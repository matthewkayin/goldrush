#pragma once

#include "util.h"
#include <cstdint>

extern uint8_t MOUSE_BUTTON_LEFT;
extern uint8_t MOUSE_BUTTON_RIGHT;

enum Font {
    FONT_HACK,
    FONT_WESTERN8,
    FONT_WESTERN16,
    FONT_WESTERN32,
    FONT_COUNT
};

enum Sprite {
    SPRITE_TILES,
    SPRITE_SELECT_RING,
    SPRITE_UNIT_MINER,
    SPRITE_COUNT
};

struct color_t {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;

    color_t() = default;
    color_t(const uint8_t& r, const uint8_t& g, const uint8_t& b, const uint8_t& a): r(r), g(g), b(b), a(a) {}
};

const color_t COLOR_WHITE(255, 255, 255, 255);
const color_t COLOR_BLACK(0, 0, 0, 255);
const color_t COLOR_SAND(244, 204, 161, 255);
const color_t COLOR_RED(230, 72, 46, 255);

enum TextAnchor {
    TEXT_ANCHOR_TOP_LEFT,
    TEXT_ANCHOR_BOTTOM_LEFT
};
const int RENDER_TEXT_CENTERED = -1;

bool engine_init(ivec2 window_size);
void engine_quit();

ivec2 engine_get_sprite_frame_size(Sprite sprite);

// Input
bool input_pump_events();
bool input_is_mouse_button_pressed(uint8_t button);
bool input_is_mouse_button_just_pressed(uint8_t button);
bool input_is_mouse_button_just_released(uint8_t button);

ivec2 input_get_mouse_position();

void input_start_text_input(const rect& text_input_rect);
void input_stop_text_input();
bool input_is_text_input_active();
void input_set_text_input_value(const char* value);
const char* input_get_text_input_value();
size_t input_get_text_input_length();

void render_clear();
void render_present();
void render_text(Font font, const char* text, color_t color, ivec2 position, TextAnchor anchor = TEXT_ANCHOR_TOP_LEFT);
void render_rect(rect r, color_t color, bool fill = false);
void render_line(ivec2 start, ivec2 end, color_t color);
void render_map(ivec2 camera_offset, int* tiles, int map_width, int map_height);
void render_sprite(ivec2 camera_offset, Sprite sprite, ivec2 frame, vec2 position);