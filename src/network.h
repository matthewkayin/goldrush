#pragma once

#include "defines.h"
#include <cstdint>

const size_t MAX_USERNAME_LENGTH = 16;
const size_t NETWORK_MAX_PLAYERS = 8;

enum NetworkStatus {
    NETWORK_STATUS_OFFLINE,
    NETWORK_STATUS_SERVER,
    NETWORK_STATUS_CONNECTING,
    NETWORK_STATUS_CONNECTED,
    NETWORK_STATUS_DISCONNECTING
};

enum PlayerStatus {
    PLAYER_STATUS_NONE,
    PLAYER_STATUS_HOST,
    PLAYER_STATUS_NOT_READY,
    PLAYER_STATUS_READY
};

struct player_t {
    char name[17];
    uint8_t status;
};

enum NetworkEventType {
    NETWORK_EVENT_CONNECTION_FAILED,
    NETWORK_EVENT_SERVER_DISCONNECTED,
    NETWORK_EVENT_JOINED_LOBBY,
    NETWORK_EVENT_LOBBY_FULL,
    NETWORK_EVENT_INVALID_VERSION,
    NETWORK_EVENT_MATCH_START
};

struct network_event_t {
    NetworkEventType type;
};

bool network_init();
void network_quit();
void network_disconnect();

bool network_is_server();
NetworkStatus network_get_status();
const player_t& network_get_player(uint8_t player_id);

void network_service();
bool network_poll_events(network_event_t* event);

bool network_server_create(const char* username);
void network_server_start_game();

bool network_client_create(const char* username, const char* server_ip);
void network_client_toggle_ready();