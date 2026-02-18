#pragma once

#include "shell/chat.h"
#include "match/state.h"
#include "match/input.h"
#include "network/types.h"
#include <vector>
#include <cstdio>

struct ReplayChatMessage {
    uint32_t turn;
    ChatMessage chat;
};

#ifdef GOLD_DEBUG
void replay_set_filename(const char* argv);
#endif
FILE* replay_file_open(int32_t lcg_seed, MapType map_type, const Noise* noise, MatchPlayer players[MAX_PLAYERS]);
void replay_file_close(FILE* file);
void replay_file_write_inputs(FILE* file, uint8_t player_id, const std::vector<MatchInput>* inputs);
void replay_file_write_chat(FILE* file, const ReplayChatMessage& message);
bool replay_file_read(const char* path, MatchState* state, std::vector<std::vector<MatchInput>>* match_inputs, std::vector<ReplayChatMessage>* match_chatlog);