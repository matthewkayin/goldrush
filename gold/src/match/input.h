#pragma once

#include "math/gmath.h"
#include "entity.h"
#include <cstdint>

enum MatchInputType {
    MATCH_INPUT_NONE,
    MATCH_INPUT_MOVE_CELL,
    MATCH_INPUT_MOVE_ENTITY,
    MATCH_INPUT_MOVE_ATTACK_CELL,
    MATCH_INPUT_MOVE_ATTACK_ENTITY,
    MATCH_INPUT_MOVE_REPAIR,
    MATCH_INPUT_MOVE_UNLOAD,
    MATCH_INPUT_MOVE_SMOKE,
    MATCH_INPUT_STOP,
    MATCH_INPUT_DEFEND,
    MATCH_INPUT_BUILD,
    MATCH_INPUT_BUILD_CANCEL,
    MATCH_INPUT_BUILDING_ENQUEUE,
    MATCH_INPUT_BUILDING_DEQUEUE,
    MATCH_INPUT_RALLY
};

struct MatchInputMove {
    uint8_t shift_command;
    ivec2 target_cell;
    EntityId target_id;
    uint8_t entity_count;
    EntityId entity_ids[SELECTION_LIMIT];
};

struct MatchInputStop {
    uint8_t entity_count;
    EntityId entity_ids[SELECTION_LIMIT];
};

struct MatchInputBuild {
    uint8_t shift_command;
    uint8_t building_type;
    ivec2 target_cell;
    uint8_t entity_count;
    EntityId entity_ids[SELECTION_LIMIT];
};

struct MatchInputBuildCancel {
    EntityId building_id;
};

struct MatchInputBuildingEnqueue {
    EntityId building_id;
    uint8_t item_type;
    uint32_t item_subtype;
};

struct MatchInputBuildingDequeue {
    EntityId building_id;
    uint8_t index;
};

struct MatchInputRally {
    ivec2 rally_point;
    uint8_t building_count;
    EntityId building_ids[SELECTION_LIMIT];
};

struct MatchInput {
    uint8_t type;
    union {
        MatchInputMove move;
        MatchInputStop stop;
        MatchInputBuild build;
        MatchInputBuildCancel build_cancel;
        MatchInputBuildingEnqueue building_enqueue;
        MatchInputBuildingDequeue building_dequeue;
        MatchInputRally rally;
    };
};

void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const MatchInput& input);
MatchInput match_input_deserialize(const uint8_t* in_buffer, size_t& in_buffer_head);