#pragma once

#include "defines.h"
#include "match/state.h"

uint32_t desync_compute_checksum(const MatchState& match_state);

#ifdef GOLD_DEBUG_DESYNC

bool desync_init(const char* desync_filename);
void desync_quit();

uint8_t* desync_read_serialized_frame(uint32_t frame_number, size_t* state_buffer_length);
void desync_compare_frames(uint8_t* state_buffer, uint8_t* state_buffer2);

#endif