#include "match_input.h"

void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const MatchInput& input) {
    out_buffer[out_buffer_length] = input.type;
    out_buffer_length++;

    switch (input.type) {
        MATCH_INPUT_NONE:
            break;
    }
}

MatchInput match_input_deserialize(const uint8_t* in_buffer, size_t& in_buffer_head) {
    MatchInput input;
    input.type = in_buffer[in_buffer_head];
    in_buffer_head++;

    switch (input.type) {
        MATCH_INPUT_NONE:
            break;
    }

    return input;
}