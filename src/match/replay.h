#pragma once

#include "state.h"
#include "input.h"
#include "network/types.h"
#include <vector>
#include <cstdio>

struct ReplayChatMessage {
    uint32_t turn;
    uint8_t player_id;
    char message[NETWORK_CHAT_BUFFER_SIZE];
};

#ifdef GOLD_DEBUG
    void replay_debug_set_file_name(char* argv);
#endif
FILE* replay_file_open(int32_t lcg_seed, const Noise& noise, MatchPlayer players[MAX_PLAYERS]);
void replay_file_close(FILE* file);
void replay_file_write_inputs(FILE* file, uint8_t player_id, const std::vector<MatchInput>* inputs);
void replay_file_write_chat(FILE* file, uint8_t player_id, uint32_t turn, const char* message);
bool replay_file_read(const char* path, MatchState* state, std::vector<std::vector<MatchInput>>* match_inputs, std::vector<ReplayChatMessage>* match_chatlog);