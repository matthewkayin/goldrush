#pragma once

#include "defines.h"

#include "match/state.h"

bool match_desync_init(const char* desync_filename);
void match_desync_quit();
uint32_t match_serialize(const MatchState& match_state);
uint8_t* match_read_serialized_frame(uint32_t frame_number, size_t* state_buffer_length);
void desync_compare_frames(uint8_t* state_buffer, uint8_t* state_buffer2);
uint32_t compute_checksum(uint8_t* data, size_t length);