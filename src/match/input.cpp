#include "match/input.h"

#include "core/logger.h"
#include <cstring>
#include <cstdio>

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
            memcpy(out_buffer + out_buffer_length, &input.build_cancel.building_id, sizeof(EntityId));
            out_buffer_length += sizeof(EntityId);
            break;
        }
        case MATCH_INPUT_BUILDING_ENQUEUE: {
            memcpy(out_buffer + out_buffer_length, &input.building_enqueue.item_type, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.building_enqueue.item_subtype, sizeof(uint32_t));
            out_buffer_length += sizeof(uint32_t);

            memcpy(out_buffer + out_buffer_length, &input.building_enqueue.building_count, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.building_enqueue.building_ids, input.building_enqueue.building_count * sizeof(EntityId));
            out_buffer_length += input.building_enqueue.building_count * sizeof(EntityId);

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
        case MATCH_INPUT_CAMO:
        case MATCH_INPUT_DECAMO: {
            memcpy(out_buffer + out_buffer_length, &input.camo.unit_count, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.camo.unit_ids, input.camo.unit_count * sizeof(EntityId));
            out_buffer_length += input.camo.unit_count * sizeof(EntityId);
            break;
        }
        case MATCH_INPUT_PATROL: {
            memcpy(out_buffer + out_buffer_length, &input.patrol.target_cell_a, sizeof(ivec2));
            out_buffer_length += sizeof(ivec2);

            memcpy(out_buffer + out_buffer_length, &input.patrol.target_cell_b, sizeof(ivec2));
            out_buffer_length += sizeof(ivec2);

            memcpy(out_buffer + out_buffer_length, &input.patrol.unit_count, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, input.patrol.unit_ids, input.patrol.unit_count * sizeof(EntityId));
            out_buffer_length += input.patrol.unit_count * sizeof(EntityId);
            break;
        }
        case MATCH_INPUT_TYPE_COUNT: {
            GOLD_ASSERT(false);
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
            memcpy(&input.build_cancel.building_id, in_buffer + in_buffer_head, sizeof(EntityId));
            in_buffer_head += sizeof(EntityId);
            break;
        }
        case MATCH_INPUT_BUILDING_ENQUEUE: {
            memcpy(&input.building_enqueue.item_type, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.building_enqueue.item_subtype, in_buffer + in_buffer_head, sizeof(uint32_t));
            in_buffer_head += sizeof(uint32_t);

            memcpy(&input.building_enqueue.building_count, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.building_enqueue.building_ids, in_buffer + in_buffer_head, input.building_enqueue.building_count * sizeof(EntityId));
            in_buffer_head += input.building_enqueue.building_count * sizeof(EntityId);
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
        case MATCH_INPUT_CAMO:
        case MATCH_INPUT_DECAMO: {
            memcpy(&input.camo.unit_count, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.camo.unit_ids, in_buffer + in_buffer_head, input.camo.unit_count * sizeof(EntityId));
            in_buffer_head += input.camo.unit_count * sizeof(EntityId);
            break;
        }
        case MATCH_INPUT_PATROL: {
            memcpy(&input.patrol.target_cell_a, in_buffer + in_buffer_head, sizeof(ivec2));
            in_buffer_head += sizeof(ivec2);

            memcpy(&input.patrol.target_cell_b, in_buffer + in_buffer_head, sizeof(ivec2));
            in_buffer_head += sizeof(ivec2);

            memcpy(&input.patrol.unit_count, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(input.patrol.unit_ids, in_buffer + in_buffer_head, input.patrol.unit_count * sizeof(EntityId));
            in_buffer_head += input.patrol.unit_count * sizeof(EntityId);
            break;
        }
        case MATCH_INPUT_TYPE_COUNT: {
            GOLD_ASSERT(false);
            break;
        }
    }

    return input;
}

const char* match_input_type_str(MatchInputType type) {
    switch (type) {
        case MATCH_INPUT_NONE:
            return "NONE";
        case MATCH_INPUT_MOVE_CELL:
            return "MOVE_CELL";
        case MATCH_INPUT_MOVE_ENTITY:
            return "MOVE_ENTITY";
        case MATCH_INPUT_MOVE_ATTACK_CELL:
            return "MOVE_ATTACK_CELL";
        case MATCH_INPUT_MOVE_ATTACK_ENTITY:
            return "MOVE_ATTACK_ENTITY";
        case MATCH_INPUT_MOVE_REPAIR:
            return "MOVE_REPAIR";
        case MATCH_INPUT_MOVE_UNLOAD:
            return "MOVE_UNLOAD";
        case MATCH_INPUT_MOVE_MOLOTOV:
            return "MOVE_MOLOTOV";
        case MATCH_INPUT_STOP:
            return "STOP";
        case MATCH_INPUT_DEFEND:
            return "DEFEND";
        case MATCH_INPUT_BUILD:
            return "BUILD";
        case MATCH_INPUT_BUILD_CANCEL:
            return "BUILD_CANCEL";
        case MATCH_INPUT_BUILDING_ENQUEUE:
            return "BUILDING_ENQUEUE";
        case MATCH_INPUT_BUILDING_DEQUEUE:
            return "BUILDING_DEQUEUE";
        case MATCH_INPUT_RALLY:
            return "RALLY";
        case MATCH_INPUT_SINGLE_UNLOAD:
            return "SINGLE_UNLOAD";
        case MATCH_INPUT_UNLOAD:
            return "UNLOAD";
        case MATCH_INPUT_CAMO:
            return "CAMO";
        case MATCH_INPUT_DECAMO:
            return "DECAMO";
        case MATCH_INPUT_PATROL:
            return "PATROL";
        case MATCH_INPUT_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

void match_input_print(char* out_ptr, const MatchInput& input) {
    out_ptr += sprintf(out_ptr, "%s | ", match_input_type_str((MatchInputType)input.type));
    switch (input.type) {
        case MATCH_INPUT_NONE:
            return;
        case MATCH_INPUT_MOVE_CELL:
        case MATCH_INPUT_MOVE_ENTITY:
        case MATCH_INPUT_MOVE_ATTACK_CELL:
        case MATCH_INPUT_MOVE_ATTACK_ENTITY:
        case MATCH_INPUT_MOVE_REPAIR:
        case MATCH_INPUT_MOVE_UNLOAD:
        case MATCH_INPUT_MOVE_MOLOTOV: {
            out_ptr += sprintf(out_ptr, "shift? %u cell <%i, %i> id %u count %u ids [", input.move.shift_command, input.move.target_cell.x, input.move.target_cell.y, input.move.target_id, input.move.entity_count);
            for (int index = 0; index < input.move.entity_count; index++) {
                out_ptr += sprintf(out_ptr, "%u,", input.move.entity_ids[index]);
            }
            out_ptr += sprintf(out_ptr, "]");
            break;
        }
        case MATCH_INPUT_STOP:
        case MATCH_INPUT_DEFEND: {
            out_ptr += sprintf(out_ptr, "count %u ids[", input.stop.entity_count);
            for (int index = 0; index < input.stop.entity_count; index++) {
                out_ptr += sprintf(out_ptr, "%u,", input.stop.entity_ids[index]);
            }
            out_ptr += sprintf(out_ptr, "]");
            break;
        }
        case MATCH_INPUT_BUILD: {
            out_ptr += sprintf(out_ptr, "shift? %u type %u cell <%i, %i> count %u ids [", input.build.shift_command, input.build.building_type, input.build.target_cell.x, input.build.target_cell.y, input.build.entity_count);
            for (int index = 0; index < input.build.entity_count; index++) {
                out_ptr += sprintf(out_ptr, "%u,", input.build.entity_ids[index]);
            }
            out_ptr += sprintf(out_ptr, "]");
            break;
        }
        case MATCH_INPUT_BUILD_CANCEL: {
            out_ptr += sprintf(out_ptr, "%u", input.build_cancel.building_id);
            break;
        }
        case MATCH_INPUT_BUILDING_ENQUEUE: {
            out_ptr += sprintf(out_ptr, "type %u subtype %u count %u ids[", input.building_enqueue.item_type, input.building_enqueue.item_subtype, input.building_enqueue.building_count);
            for (int index = 0; index < input.building_enqueue.building_count; index++) {
                out_ptr += sprintf(out_ptr, "%u ", input.building_enqueue.building_ids[index]);
            }
            out_ptr += sprintf(out_ptr, "]");
            break;
        }
        case MATCH_INPUT_BUILDING_DEQUEUE: {
            out_ptr += sprintf(out_ptr, "building id %u index %u", input.building_dequeue.building_id, input.building_dequeue.index);
            break;
        }
        case MATCH_INPUT_RALLY: {
            out_ptr += sprintf(out_ptr, "point <%i, %i> count %u ids [", input.rally.rally_point.x, input.rally.rally_point.y, input.rally.building_count);
            for (int index = 0; index < input.rally.building_count; index++) {
                out_ptr += sprintf(out_ptr, "%u,", input.rally.building_ids[index]);
            }
            out_ptr += sprintf(out_ptr, "]");
            break;
        }
        case MATCH_INPUT_SINGLE_UNLOAD: {
            out_ptr += sprintf(out_ptr, "entity id %u", input.single_unload.entity_id);
            break;
        }
        case MATCH_INPUT_UNLOAD: {
            out_ptr += sprintf(out_ptr, "count %u ids[", input.unload.carrier_count);
            for (int index = 0; index < input.unload.carrier_count; index++) {
                out_ptr += sprintf(out_ptr, "%u,", input.unload.carrier_ids[index]);
            }
            out_ptr += sprintf(out_ptr, "]");
            break;
        }
        case MATCH_INPUT_CAMO: 
        case MATCH_INPUT_DECAMO: {
            out_ptr += sprintf(out_ptr, "count %u ids[", input.camo.unit_count);
            for (int index = 0; index < input.camo.unit_count; index++) {
                out_ptr += sprintf(out_ptr, "%u,", input.camo.unit_ids[index]);
            }
            out_ptr += sprintf(out_ptr, "]");
            break;
        }
        case MATCH_INPUT_PATROL: {
            out_ptr += sprintf(out_ptr, "<%i, %i> -> <%i, %i> count %u ids[",
                    input.patrol.target_cell_a.x, input.patrol.target_cell_a.y,
                    input.patrol.target_cell_b.x, input.patrol.target_cell_b.y,
                    input.patrol.unit_count);
            for (uint8_t index = 0; index < input.patrol.unit_count; index++) {
                out_ptr += sprintf(out_ptr, "%u, ", input.patrol.unit_ids[index]);
            }
            out_ptr += sprintf(out_ptr, "]");
            break;
        }
        case MATCH_INPUT_TYPE_COUNT: {
            GOLD_ASSERT(false);
            break;
        }
    }
}