#include "match_input.h"

#include <cstring>

void input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input) {
    out_buffer[out_buffer_length] = input.type;
    out_buffer_length++;

    switch (input.type) {
        case INPUT_MOVE: {
            memcpy(out_buffer + out_buffer_length, &input.move.target_cell, sizeof(ivec2));
            out_buffer_length += sizeof(ivec2);

            out_buffer[out_buffer_length] = input.move.unit_count;
            out_buffer_length += 1;

            memcpy(out_buffer + out_buffer_length, input.move.unit_ids, input.move.unit_count * sizeof(uint8_t));
            out_buffer_length += input.move.unit_count * sizeof(uint8_t);
            break;
        }
        case INPUT_STOP: {
            out_buffer[out_buffer_length] = input.stop.unit_count;
            out_buffer_length += 1;

            memcpy(out_buffer + out_buffer_length, input.stop.unit_ids, input.stop.unit_count * sizeof(uint8_t));
            out_buffer_length += input.stop.unit_count * sizeof(uint8_t);
            break;
        }
        case INPUT_BUILD: {
            memcpy(out_buffer + out_buffer_length, &input.build, sizeof(input_build_t));
            break;
        }
        default:
            break;
    }
}

input_t input_deserialize(uint8_t* in_buffer, size_t& in_buffer_head) {
    input_t input;
    input.type = in_buffer[in_buffer_head];
    in_buffer_head++;

    switch (input.type) {
        case INPUT_MOVE: {
            memcpy(&input.move.target_cell, in_buffer + in_buffer_head, sizeof(ivec2));
            in_buffer_head += sizeof(ivec2);

            input.move.unit_count = in_buffer[in_buffer_head];
            memcpy(input.move.unit_ids, in_buffer + in_buffer_head + 1, input.move.unit_count * sizeof(uint8_t));
            in_buffer_head += 1 + input.move.unit_count;
            break;
        }
        case INPUT_STOP: {
            input.stop.unit_count = in_buffer[in_buffer_head];
            memcpy(input.stop.unit_ids, in_buffer + in_buffer_head + 1, input.stop.unit_count * sizeof(uint8_t));
            in_buffer_head += 1 + input.stop.unit_count;
            break;
        }
        case INPUT_BUILD: {
            memcpy(&input.build, in_buffer + in_buffer_head, sizeof(input_build_t));
            in_buffer_head += sizeof(input_build_t);
            break;
        }
        default:
            break;
    }

    return input;
}