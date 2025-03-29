#include "match/input.h"

#include <cstring>

void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const MatchInput& input) {
    out_buffer[out_buffer_length] = input.type;
    out_buffer_length++;

    switch (input.type) {
        MATCH_INPUT_NONE:
            break;
        case MATCH_INPUT_MOVE_CELL:
        case MATCH_INPUT_MOVE_ENTITY:
        case MATCH_INPUT_MOVE_ATTACK_CELL:
        case MATCH_INPUT_MOVE_ATTACK_ENTITY:
        case MATCH_INPUT_MOVE_REPAIR:
        case MATCH_INPUT_MOVE_UNLOAD:
        case MATCH_INPUT_MOVE_SMOKE: {
            memcpy(out_buffer + out_buffer_length, &input.move.shift_command, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.move.target_cell, sizeof(ivec2));
            out_buffer_length += sizeof(ivec2);

            memcpy(out_buffer + out_buffer_length, &input.move.target_id, sizeof(EntityId));
            out_buffer_length += sizeof(EntityId);

            memcpy(out_buffer + out_buffer_length, &input.move.entity_count, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.move.entity_ids, input.move.entity_count * sizeof(EntityId));
            out_buffer_length += input.move.entity_count * sizeof(EntityId);
            break;
        }
    }
}

MatchInput match_input_deserialize(const uint8_t* in_buffer, size_t& in_buffer_head) {
    MatchInput input;
    input.type = in_buffer[in_buffer_head];
    in_buffer_head++;

    switch (input.type) {
        MATCH_INPUT_NONE:
            break;
        case MATCH_INPUT_MOVE_CELL:
        case MATCH_INPUT_MOVE_ENTITY:
        case MATCH_INPUT_MOVE_ATTACK_CELL:
        case MATCH_INPUT_MOVE_ATTACK_ENTITY:
        case MATCH_INPUT_MOVE_REPAIR:
        case MATCH_INPUT_MOVE_UNLOAD:
        case MATCH_INPUT_MOVE_SMOKE: {
            memcpy(&input.move.shift_command, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.move.target_cell, in_buffer + in_buffer_head, sizeof(ivec2));
            in_buffer_head += sizeof(ivec2);

            memcpy(&input.move.target_id, in_buffer + in_buffer_head, sizeof(EntityId));
            in_buffer_head += sizeof(EntityId);

            memcpy(&input.move.entity_count, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.move.entity_ids, in_buffer + in_buffer_head, input.move.entity_count * sizeof(EntityId));
            in_buffer_head += input.move.entity_count * sizeof(EntityId);
            break;
        }
    }

    return input;
}