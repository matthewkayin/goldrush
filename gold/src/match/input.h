#pragma once

#include "math/gmath.h"
#include <cstdint>

enum MatchInputType {
    MATCH_INPUT_NONE,
    MATCH_INPUT_MOVE_CELL,
    MATCH_INPUT_MOVE_ENTITY,
    MATCH_INPUT_MOVE_ATTACK_CELL,
    MATCH_INPUT_MOVE_ATTACK_ENTITY,
    MATCH_INPUT_MOVE_REPAIR,
    MATCH_INPUT_MOVE_UNLOAD,
    MATCH_INPUT_MOVE_SMOKE
};

struct MatchInputMove {
    uint8_t shift_command;
    ivec2 target_cell;
    EntityId target_id;
    uint8_t entity_count;
    EntityId entity_ids[SELECTION_LIMIT];
};

struct MatchInput {
    uint8_t type;
    union {
        MatchInputMove move;
    };
};

void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const MatchInput& input);
MatchInput match_input_deserialize(const uint8_t* in_buffer, size_t& in_buffer_head);