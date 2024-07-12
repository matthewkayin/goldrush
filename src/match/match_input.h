#pragma once

#include "defines.h"
#include "util.h"
#include <cstdint>

const uint32_t INPUT_MAX_SIZE = 201;

enum InputType {
    INPUT_NONE,
    INPUT_MOVE,
    INPUT_STOP,
    INPUT_BUILD
};

struct input_move_t {
    ivec2 target_cell;
    uint8_t unit_count;
    id_t unit_ids[MAX_UNITS];
};

struct input_stop_t {
    uint8_t unit_count;
    id_t unit_ids[MAX_UNITS];
};

struct input_build_t {
    id_t unit_id;
    uint8_t building_type;
    ivec2 target_cell;
};

struct input_t {
    uint8_t type;
    union {
        input_move_t move;
        input_stop_t stop;
        input_build_t build;
    };
};

void input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input);
input_t input_deserialize(uint8_t* in_buffer, size_t& in_buffer_head);