#pragma once

#include "defines.h"
#include "match/state.h"
#include "bot/bot.h"

// The full state buffer contains the network message type 2. the frame number and 3. the serialized frame
constexpr size_t DESYNC_BUFFER_SIZE = sizeof(MatchState) + (MAX_PLAYERS * sizeof(Bot));
constexpr size_t DESYNC_STATE_BUFFER_HEADER_SIZE = sizeof(uint8_t) + sizeof(uint32_t);
constexpr size_t DESYNC_STATE_BUFFER_LENGTH = DESYNC_STATE_BUFFER_HEADER_SIZE + DESYNC_BUFFER_SIZE;

#ifdef GOLD_DEBUG

bool desync_init(const char* desync_foldername);
void desync_quit();

void desync_write_frame(uint8_t* data, uint32_t frame);
uint8_t* desync_read_frame(uint32_t frame_number);
void desync_delete_frame(uint32_t frame);
void desync_send_frame(uint32_t frame);
void desync_compare_frames(uint8_t* state_buffer, uint8_t* state_buffer2);

#else

#define desync_write_frame(data, frame)
#define desync_read_frame(frame_number)
#define desync_delete_frame(frame)
#define send_send_frame(frame)
#define desync_compare_frames(state_buffer, state_buffer2)

#endif

uint32_t desync_get_checksum_frequency();