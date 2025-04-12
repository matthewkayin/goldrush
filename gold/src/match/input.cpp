#include "match/input.h"

#include "core/logger.h"
#include <cstring>

void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const MatchInput& input) {
    out_buffer[out_buffer_length] = input.type;
    out_buffer_length++;

    switch (input.type) {
        case MATCH_INPUT_NONE:
            break;
        case MATCH_INPUT_MOVE_CELL:
        case MATCH_INPUT_MOVE_ENTITY:
        case MATCH_INPUT_MOVE_ATTACK_CELL:
        case MATCH_INPUT_MOVE_ATTACK_ENTITY:
        case MATCH_INPUT_MOVE_REPAIR:
        case MATCH_INPUT_MOVE_UNLOAD:
        case MATCH_INPUT_MOVE_MOLOTOV: {
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
        case MATCH_INPUT_STOP:
        case MATCH_INPUT_DEFEND: {
            memcpy(out_buffer + out_buffer_length, &input.stop.entity_count, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.stop.entity_ids, input.stop.entity_count * sizeof(EntityId));
            out_buffer_length += input.stop.entity_count * sizeof(EntityId);
            break;
        }
        case MATCH_INPUT_BUILD: {
            memcpy(out_buffer + out_buffer_length, &input.build.shift_command, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.build.building_type, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.build.target_cell, sizeof(ivec2));
            out_buffer_length += sizeof(ivec2);

            memcpy(out_buffer + out_buffer_length, &input.build.entity_count, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.build.entity_ids, input.build.entity_count * sizeof(EntityId));
            out_buffer_length += input.build.entity_count * sizeof(EntityId);
            break;
        }
        case MATCH_INPUT_BUILD_CANCEL: {
            memcpy(out_buffer + out_buffer_length, &input.build_cancel.building_id, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);
            break;
        }
        case MATCH_INPUT_BUILDING_ENQUEUE: {
            memcpy(out_buffer + out_buffer_length, &input.building_enqueue.building_id, sizeof(EntityId));
            out_buffer_length += sizeof(EntityId);

            memcpy(out_buffer + out_buffer_length, &input.building_enqueue.item_type, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.building_enqueue.item_subtype, sizeof(uint32_t));
            out_buffer_length += sizeof(uint32_t);
            break;
        }
        case MATCH_INPUT_BUILDING_DEQUEUE: {
            memcpy(out_buffer + out_buffer_length, &input.building_dequeue.building_id, sizeof(EntityId));
            out_buffer_length += sizeof(EntityId);

            memcpy(out_buffer + out_buffer_length, &input.building_dequeue.index, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);
            break;
        }
        case MATCH_INPUT_RALLY: {
            memcpy(out_buffer + out_buffer_length, &input.rally.rally_point, sizeof(ivec2));
            out_buffer_length += sizeof(ivec2);

            memcpy(out_buffer + out_buffer_length, &input.rally.building_count, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.rally.building_ids, input.rally.building_count * sizeof(EntityId));
            out_buffer_length += input.rally.building_count * sizeof(EntityId);
            break;
        }
        case MATCH_INPUT_SINGLE_UNLOAD: {
            memcpy(out_buffer + out_buffer_length, &input.single_unload.entity_id, sizeof(EntityId));
            out_buffer_length += sizeof(EntityId);
            break;
        }
        case MATCH_INPUT_UNLOAD: {
            memcpy(out_buffer + out_buffer_length, &input.unload.carrier_count, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.unload.carrier_ids, input.unload.carrier_count * sizeof(EntityId));
            out_buffer_length += input.unload.carrier_count * sizeof(EntityId);
            break;
        }
    }
}

MatchInput match_input_deserialize(const uint8_t* in_buffer, size_t& in_buffer_head) {
    MatchInput input;
    input.type = in_buffer[in_buffer_head];
    in_buffer_head++;

    switch (input.type) {
        case MATCH_INPUT_NONE:
            break;
        case MATCH_INPUT_MOVE_CELL:
        case MATCH_INPUT_MOVE_ENTITY:
        case MATCH_INPUT_MOVE_ATTACK_CELL:
        case MATCH_INPUT_MOVE_ATTACK_ENTITY:
        case MATCH_INPUT_MOVE_REPAIR:
        case MATCH_INPUT_MOVE_UNLOAD:
        case MATCH_INPUT_MOVE_MOLOTOV: {
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
        case MATCH_INPUT_STOP:
        case MATCH_INPUT_DEFEND: {
            memcpy(&input.stop.entity_count, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.stop.entity_ids, in_buffer + in_buffer_head, input.stop.entity_count * sizeof(EntityId));
            in_buffer_head += input.stop.entity_count * sizeof(EntityId);
            break;
        }
        case MATCH_INPUT_BUILD: {
            memcpy(&input.build.shift_command, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.build.building_type, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.build.target_cell, in_buffer + in_buffer_head, sizeof(ivec2));
            in_buffer_head += sizeof(ivec2);

            memcpy(&input.build.entity_count, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.build.entity_ids, in_buffer + in_buffer_head, input.build.entity_count * sizeof(EntityId));
            in_buffer_head += input.build.entity_count * sizeof(EntityId);
            break;
        }
        case MATCH_INPUT_BUILD_CANCEL: {
            memcpy(&input.build_cancel.building_id, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);
            break;
        }
        case MATCH_INPUT_BUILDING_ENQUEUE: {
            memcpy(&input.building_enqueue.building_id, in_buffer + in_buffer_head, sizeof(EntityId));
            in_buffer_head += sizeof(EntityId);

            memcpy(&input.building_enqueue.item_type, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.building_enqueue.item_subtype, in_buffer + in_buffer_head, sizeof(uint32_t));
            in_buffer_head += sizeof(uint32_t);
            break;
        }
        case MATCH_INPUT_BUILDING_DEQUEUE: {
            memcpy(&input.building_dequeue.building_id, in_buffer + in_buffer_head, sizeof(EntityId));
            in_buffer_head += sizeof(EntityId);

            memcpy(&input.building_dequeue.index, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);
            break;
        }
        case MATCH_INPUT_RALLY: {
            memcpy(&input.rally.rally_point, in_buffer + in_buffer_head, sizeof(ivec2));
            in_buffer_head += sizeof(ivec2);

            memcpy(&input.rally.building_count, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.rally.building_ids, in_buffer + in_buffer_head, input.rally.building_count * sizeof(EntityId));
            in_buffer_head += input.rally.building_count * sizeof(EntityId);
            break;
        }
        case MATCH_INPUT_SINGLE_UNLOAD: {
            memcpy(&input.single_unload.entity_id, in_buffer + in_buffer_head, sizeof(EntityId));
            in_buffer_head += sizeof(EntityId);
            break;
        }
        case MATCH_INPUT_UNLOAD: {
            memcpy(&input.unload.carrier_count, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.unload.carrier_ids, in_buffer + in_buffer_head, input.unload.carrier_count * sizeof(EntityId));
            in_buffer_head += input.unload.carrier_count * sizeof(EntityId);
            break;
        }
    }

    return input;
}