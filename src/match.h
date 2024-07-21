#pragma once

#include "defines.h"
#include "util.h"
#include <cstdint>
#include <vector>

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
    UI_MODE_NONE
};

struct match_state_t {
    UiMode ui_mode;

    std::vector<std::vector<input_t>> inputs[MAX_PLAYERS];
    std::vector<input_t> input_queue;
    uint32_t tick_timer;

    std::vector<int> map_tiles;
    std::vector<uint16_t> map_cells;
    int map_width;
    int map_height;

    ivec2 camera_offset;
};

match_state_t match_init();
void match_update(match_state_t& state);

void input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input);
input_t input_deserialize(uint8_t* in_buffer, size_t& in_buffer_head);