#pragma once

#include "defines.h"
#include "match/state.h"
#include "bot/bot.h"

#ifdef GOLD_DEBUG

bool desync_init(const char* desync_foldername);
void desync_quit();

uint32_t desync_compute_buffer_checksum(uint8_t* data, size_t length);
void desync_send_serialized_frame(uint32_t frame);
void desync_delete_serialized_frame(uint32_t frame);
uint8_t* desync_read_serialized_frame(uint32_t frame_number, size_t* state_buffer_length);
void desync_compare_frames(uint8_t* state_buffer, uint8_t* state_buffer2);

#else

#define desync_send_serialized_frame(frame)
#define desync_delete_serialized_frame(frame)

#endif

uint32_t desync_compute_match_checksum(const MatchState* match_state, const Bot* bots, uint32_t frame);
uint32_t desync_get_checksum_frequency();