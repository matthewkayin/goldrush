#pragma once

#include <cstdint>

enum MatchInputType {
    MATCH_INPUT_NONE
};

struct MatchInput {
    uint8_t type;
};

void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const MatchInput& input);
MatchInput match_input_deserialize(const uint8_t* in_buffer, size_t& in_buffer_head);