#pragma once

#include "defines.h"
#include "util.h"
#include "sprite.h"
#include <cstdint>
#include <vector>
#include <string>

const uint16_t CELL_EMPTY = 65535;
const int UI_HEIGHT = 88;
const uint32_t MAX_UNITS = 200;

enum InputType {
    INPUT_NONE,
    INPUT_MOVE,
    INPUT_STOP,
    INPUT_BUILD,
    INPUT_BUILD_CANCEL
};

struct input_move_t {
    ivec2 target_cell;
    uint8_t unit_count;
    uint16_t unit_ids[MAX_UNITS];
};

struct input_stop_t {
    uint8_t unit_count;
    uint16_t unit_ids[MAX_UNITS];
};

struct input_build_t {
    uint16_t unit_id;
    uint8_t building_type;
    ivec2 target_cell;
};

struct input_build_cancel_t {
    uint16_t building_id;
};

struct input_t {
    uint8_t type;
    union {
        input_move_t move;
        input_stop_t stop;
        input_build_t build;
        input_build_cancel_t build_cancel;
    };
};

enum UiMode {
    UI_MODE_NOT_STARTED,
    UI_MODE_NONE,
    UI_MODE_SELECTING,
    UI_MODE_MINIMAP_DRAG,
    UI_MODE_BUILDING_PLACE
};

enum UiButtonset {
    UI_BUTTONSET_NONE,
    UI_BUTTONSET_UNIT,
    UI_BUTTONSET_MINER,
    UI_BUTTONSET_BUILD,
    UI_BUTTONSET_CANCEL
};

struct match_state_t {
    UiMode ui_mode;
    UiButtonset ui_buttonset;
    ivec2 camera_offset;
    std::string ui_status_message;
    uint32_t ui_status_timer;

    std::vector<std::vector<input_t>> inputs[MAX_PLAYERS];
    std::vector<input_t> input_queue;
    uint32_t tick_timer;

    std::vector<int> map_tiles;
    std::vector<uint16_t> map_cells;
    int map_width;
    int map_height;

    uint32_t player_gold[MAX_PLAYERS];
};

match_state_t match_init();
void match_update(match_state_t& state);

void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input);
input_t match_input_deserialize(uint8_t* in_buffer, size_t& in_buffer_head);

void match_ui_show_status(match_state_t& state, const char* message);
UiButton match_get_ui_button(const match_state_t& state, int index);
int match_get_ui_button_hovered(const match_state_t& state);
const rect_t& match_get_ui_button_rect(int index);
bool match_is_mouse_in_ui();
ivec2 match_camera_clamp(const ivec2& camera_offset, int map_width, int map_height);
ivec2 match_camera_centered_on_cell(const ivec2& cell);