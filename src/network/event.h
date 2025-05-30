#pragma once

#include "types.h"
#include "match/noise.h"

enum NetworkEventType {
    NETWORK_EVENT_LOBBY_CONNECTION_FAILED,
    NETWORK_EVENT_LOBBY_CREATED,
    NETWORK_EVENT_LOBBY_JOIN_INVALID_VERSION,
    NETWORK_EVENT_LOBBY_JOIN_SUCCESS,
    NETWORK_EVENT_PLAYER_DISCONNECTED,
    NETWORK_EVENT_PLAYER_CONNECTED,
    NETWORK_EVENT_CHAT,
    NETWORK_EVENT_MATCH_LOAD,
    NETWORK_EVENT_INPUT
};

struct NetworkEventPlayerDisconnected {
    uint8_t player_id;
};

struct NetworkEventPlayerConnected {
    uint8_t player_id;
};

struct NetworkEventChat {
    char message[NETWORK_CHAT_BUFFER_SIZE];
    uint8_t player_id;
};

struct NetworkEventMatchLoad {
    int32_t lcg_seed;
    Noise noise;
};

struct NetworkEventInput {
    uint8_t in_buffer[NETWORK_INPUT_BUFFER_SIZE];
    uint8_t in_buffer_length;
    uint8_t player_id;
};

struct NetworkEvent {
    NetworkEventType type;
    union {
        NetworkEventPlayerDisconnected player_disconnected;
        NetworkEventPlayerConnected player_connected;
        NetworkEventChat chat;
        NetworkEventMatchLoad match_load;
        NetworkEventInput input;
    };
};

bool network_poll_events(NetworkEvent* event);
void network_push_event(NetworkEvent event);