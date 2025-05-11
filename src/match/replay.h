#pragma once

#include "state.h"
#include "input.h"
#include <vector>
#include <cstdio>

#ifdef GOLD_DEBUG
    void replay_debug_set_file_name(char* argv);
#endif
FILE* replay_file_open(int32_t lcg_seed, const Noise& noise, MatchPlayer players[MAX_PLAYERS]);
void replay_file_close(FILE* file);
void replay_file_write_inputs(FILE* file, uint8_t player_id, const std::vector<MatchInput>* inputs);
bool replay_file_read(std::vector<std::vector<MatchInput>>* match_inputs, MatchState* state, const char* path);