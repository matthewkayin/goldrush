#pragma once

#include "shell/chat.h"
#include "match/state.h"
#include "match/input.h"
#include "network/types.h"
#include <vector>
#include <cstdio>

enum ReplayEntryType: uint8_t {
    REPLAY_ENTRY_INPUT,
    REPLAY_ENTRY_CHAT,
    REPLAY_ENTRY_DISCONNECT,
    REPLAY_ENTRY_NEW_TURN
};

struct ReplayEntry {
    ReplayEntryType type;
    union {
        MatchInput input;
        ChatMessage chat_message;
        uint8_t disconnect_player_id;
    };
};

struct ReplayChatMessage {
    uint32_t turn;
    ChatMessage chat;
};

#ifdef GOLD_DEBUG
void replay_set_filename(const char* argv);
#endif
FILE* replay_file_open(int32_t lcg_seed, MapType map_type, const Noise* noise, MatchPlayer players[MAX_PLAYERS]);
void replay_file_close(FILE* file);
void replay_file_write_entry(FILE* file, const ReplayEntry& entry);
bool replay_file_read(const char* path, MatchState& state, std::vector<std::vector<ReplayEntry>>* replay_entries);