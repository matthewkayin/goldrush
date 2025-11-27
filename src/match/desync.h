#pragma once

#include "defines.h"

#include "match/state.h"

bool match_desync_init(const char* desync_filename);
void match_desync_quit();
uint32_t match_checksum(uint32_t frame_number, const MatchState& match_state);