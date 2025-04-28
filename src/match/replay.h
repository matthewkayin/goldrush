#pragma once

#include "state.h"
#include "input.h"
#include <vector>
#include <cstdio>

FILE* replay_file_open(int32_t lcg_seed, const Noise& noise, MatchPlayer players[MAX_PLAYERS]);
void replay_file_close(FILE* file);
void replay_file_write_inputs(FILE* file, uint8_t player_id, const std::vector<MatchInput>* inputs);
bool replay_file_read(std::queue<std::vector<MatchInput>>* match_inputs, MatchState* state, const char* path);